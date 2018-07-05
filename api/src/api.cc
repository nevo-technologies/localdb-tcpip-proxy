#include <stdio.h>
#include <wchar.h>
#include <node.h>
#define LOCALDB_DEFINE_PROXY_FUNCTIONS
#include <sqlncli.h>

namespace toy
{
	using v8::Array;
	using v8::Boolean;
	using v8::Exception;
	using v8::FunctionCallbackInfo;
	using v8::Isolate;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::String;
	using v8::Value;

	const LONG64 FTIME_ZERO = -11644473600000; // 1601-01-01

	class Helper {
	public:
		Helper(Isolate*);

		// validates an argument as an instance name
		const PCWSTR validateName(const FunctionCallbackInfo<Value>&, int = 0);

		// js factory
		Local<Boolean> boolean(bool);
		template<class T>
		Local<Number> number(T);
		Local<String> string(const char*); // utf8
		Local<String> string(const WCHAR*); // utf16
		Local<Array> array(int);
		Local<Object> object();

		void setProp(Local<Object>, const char*, Local<Value>);
		void throwError(Local<Value>);
		// throw an Error on failed call
		bool apiFailed(HRESULT, const char* method);

	private:
		Isolate* isolate;
		char16_t nameBuf[1 + MAX_LOCALDB_INSTANCE_NAME_LENGTH];
	};

	bool nonBlank(const WCHAR* s) {
		return s && *s;
	}

	LONG64 ftime(const FILETIME t) {
		ULONG64 ticks = (((ULONG64) t.dwHighDateTime) << 32) | t.dwLowDateTime;
		return FTIME_ZERO + (ticks / 10000);
	}

	/**
	 * returns information about an instance (undefined if none)
	 * connectionString: 'np:' {pipe-name}
	 */
	void describeInstance(const FunctionCallbackInfo<Value>& args) {
		Helper h(args.GetIsolate());
		const PCWSTR wname = h.validateName(args);
		if (!wname) return;

		LocalDBInstanceInfo info;
		HRESULT hr = LocalDBGetInstanceInfo(wname, &info, sizeof(LocalDBInstanceInfo));
		if (h.apiFailed(hr, "LocalDBGetInstanceInfo")) return;

		bool exists = info.bExists, automatic = info.bIsAutomatic;
		if (!(exists || automatic)) return;

		Local<Object> res = h.object();

		h.setProp(res, "name", h.string(info.wszInstanceName));
		if (nonBlank(info.wszSharedInstanceName))
			h.setProp(res, "sharedName", h.string(info.wszSharedInstanceName));
		if (nonBlank(info.wszConnection))
			h.setProp(res, "connectionString", h.string(info.wszConnection));
		h.setProp(res, "running", h.boolean(info.bIsRunning));
		h.setProp(res, "automatic", h.boolean(automatic));
		h.setProp(res, "exists", h.boolean(exists));
		h.setProp(res, "corrupted", h.boolean(info.bConfigurationCorrupted));
		if (exists) {
			char version[64];
			sprintf(version, "%d.%d.%d.%d", info.dwMajor, info.dwMinor, info.dwBuild, info.dwRevision);

			h.setProp(res, "version", h.string(version));
			h.setProp(res, "lastStarted", h.number(ftime(info.ftLastStartDateUTC)));
			h.setProp(res, "ownerSID", h.string(info.wszOwnerSID));
		}

		args.GetReturnValue().Set(res);
	}

	/**
	 * returns connectionString of a started (or running) instance
	 */
	void startInstance(const FunctionCallbackInfo<Value>& args) {
		Helper h(args.GetIsolate());
		const PCWSTR wname = h.validateName(args);
		if (!wname) return;

		WCHAR connectionString[LOCALDB_MAX_SQLCONNECTION_BUFFER_SIZE];
		DWORD len = LOCALDB_MAX_SQLCONNECTION_BUFFER_SIZE;
		HRESULT hr = LocalDBStartInstance(wname, 0, connectionString, &len);
		if (h.apiFailed(hr, "LocalDBStartInstance")) return;

		args.GetReturnValue().Set(h.string(connectionString));
	}

	/**
	 * stops an instance (running or otherwise)
	 * args[0]: instance name
	 * args[1]?: options of the form:
	 *   timeout?: in seconds (default 10) - 0 requests shutdown without waiting for a response
	 *   noWait?: SHUTDOWN WITH NOWAIT (default false)
	 *   kill?: kill the server process (default false) - takes precedence over noWait
	 */
	void stopInstance(const FunctionCallbackInfo<Value>& args) {
		Helper h(args.GetIsolate());
		const PCWSTR wname = h.validateName(args);
		if (!wname) return;

		DWORD flags = 0;
		ULONG timeout = 10;
		if (args.Length() > 1) {
			if (!args[1]->IsObject()) {
				h.throwError(Exception::RangeError(h.string("options not an object")));
				return;
			}
			Local<Object> opts = args[1].As<Object>();
			Local<Value> prop;
			// timeout
			if ((prop = opts->Get(h.string("timeout")))->IsNumber()) {
				// TODO - NaN, etc.
				double value = prop->NumberValue();
				if (value < 0) {
					h.throwError(Exception::RangeError(h.string("negative timeout")));
					return;
				}
				timeout = (ULONG) value;
			}
			else if (!prop->IsNullOrUndefined()) {
				h.throwError(Exception::RangeError(h.string("timeout not a number")));
				return;
			}
			// kill
			if (!(prop = opts->Get(h.string("kill")))->IsNullOrUndefined())
				flags = prop->BooleanValue() ? LOCALDB_SHUTDOWN_KILL_PROCESS : flags;
			// noWait
			else if (!(prop = opts->Get(h.string("noWait")))->IsNullOrUndefined())
				flags = prop->BooleanValue() ? LOCALDB_SHUTDOWN_WITH_NOWAIT : flags;
		}

		HRESULT hr = LocalDBStopInstance(wname, flags, timeout);
		h.apiFailed(hr, "LocalDBStopInstance");
	}

	void listInstanceNames(const FunctionCallbackInfo<Value>& args) {
		Helper h(args.GetIsolate());

		DWORD n = 0;
		// count
		HRESULT hr = LocalDBGetInstances(NULL, &n);
		if (LOCALDB_ERROR_INSUFFICIENT_BUFFER != hr && h.apiFailed(hr, "LocalDBGetInstances")) return;
		if (n < 1) {
			args.GetReturnValue().Set(h.array(0));
			return;
		}

		Local<Value> oom = Exception::Error(h.string("Failed to allocate name buffer"));
		PTLocalDBInstanceName buf = (PTLocalDBInstanceName) malloc(n * sizeof(TLocalDBInstanceName));
		if (!buf) { // now what
			h.throwError(oom);
			return;
		}

		// actual names
		if (h.apiFailed((LocalDBGetInstances(buf, &n)), "LocalDBGetInstances")) {
			free(buf);
			return;
		}

		Local<Array> names = h.array((int) n);
		for (int i = 0; i < n; i++)
			names->Set(h.number(i), h.string(buf[i]));
		free(buf);
		
		args.GetReturnValue().Set(names);
	}

	void init(Local<Object> exports) {
		NODE_SET_METHOD(exports, "describeInstance", describeInstance);
		NODE_SET_METHOD(exports, "startInstance", startInstance);
		NODE_SET_METHOD(exports, "stopInstance", stopInstance);
		NODE_SET_METHOD(exports, "listInstanceNames", listInstanceNames);
	}

	NODE_MODULE(NODE_GYP_MODULE_NAME, init)


	Helper::Helper(Isolate* iso) {
		isolate = iso;
	}

	const PCWSTR Helper::validateName(const FunctionCallbackInfo<Value>& args, int n) {
		if (args.Length() < n || !args[n]->IsString()) {
			isolate->ThrowException(Exception::RangeError(string("expected an instance name")));
			return NULL;
		}
		
		Local<String> name = args[n].As<String>();
		if (name->Length() < 1 || name->Length() > MAX_LOCALDB_INSTANCE_NAME_LENGTH) {
			throwError(Exception::RangeError(string("Invalid instance name")));
			return NULL;
		}
		name->Write((uint16_t*) nameBuf);
		return (PCWSTR) nameBuf;
	}

	Local<Boolean> Helper::boolean(bool b) {
		return Boolean::New(isolate, b);
	}
	template<class T>
	Local<Number> Helper::number(T n) {
		return Number::New(isolate, (double) n);
	}
	Local<String> Helper::string(const char* s) {
		return String::NewFromUtf8(isolate, s);
	}
	Local<String> Helper::string(const WCHAR* s) {
		return String::NewFromTwoByte(isolate, (uint16_t*) s, String::NewStringType::kNormalString);
	}
	Local<Array> Helper::array(int len) {
		return Array::New(isolate, len);
	}
	Local<Object> Helper::object() {
		return Object::New(isolate);
	}

	void Helper::setProp(Local<Object> tgt, const char* name, Local<Value> value) {
		tgt->Set(string(name), value);
	}
	void Helper::throwError(Local<Value> e) {
		isolate->ThrowException(e);
	}
	bool Helper::apiFailed(HRESULT hr, const char* method) {
		if (SUCCEEDED(hr)) return false;

		char msg[256];
		char* code = NULL;
		// grepped from sqlnclient.h - TODO could use LocalDBFormatMessage
		switch (hr) {
			case LOCALDB_ERROR_NOT_INSTALLED: code = "LOCALDB_ERROR_NOT_INSTALLED"; break;
			case LOCALDB_ERROR_CANNOT_CREATE_INSTANCE_FOLDER: code = "LOCALDB_ERROR_CANNOT_CREATE_INSTANCE_FOLDER"; break;
			case LOCALDB_ERROR_INVALID_PARAMETER: code = "LOCALDB_ERROR_INVALID_PARAMETER"; break;
			case LOCALDB_ERROR_INSTANCE_EXISTS_WITH_LOWER_VERSION: code = "LOCALDB_ERROR_INSTANCE_EXISTS_WITH_LOWER_VERSION"; break;
			case LOCALDB_ERROR_CANNOT_GET_USER_PROFILE_FOLDER: code = "LOCALDB_ERROR_CANNOT_GET_USER_PROFILE_FOLDER"; break;
			case LOCALDB_ERROR_INSTANCE_FOLDER_PATH_TOO_LONG: code = "LOCALDB_ERROR_INSTANCE_FOLDER_PATH_TOO_LONG"; break;
			case LOCALDB_ERROR_CANNOT_ACCESS_INSTANCE_FOLDER: code = "LOCALDB_ERROR_CANNOT_ACCESS_INSTANCE_FOLDER"; break;
			case LOCALDB_ERROR_CANNOT_ACCESS_INSTANCE_REGISTRY: code = "LOCALDB_ERROR_CANNOT_ACCESS_INSTANCE_REGISTRY"; break;
			case LOCALDB_ERROR_UNKNOWN_INSTANCE: code = "LOCALDB_ERROR_UNKNOWN_INSTANCE"; break;
			case LOCALDB_ERROR_INTERNAL_ERROR: code = "LOCALDB_ERROR_INTERNAL_ERROR"; break;
			case LOCALDB_ERROR_CANNOT_MODIFY_INSTANCE_REGISTRY: code = "LOCALDB_ERROR_CANNOT_MODIFY_INSTANCE_REGISTRY"; break;
			case LOCALDB_ERROR_SQL_SERVER_STARTUP_FAILED: code = "LOCALDB_ERROR_SQL_SERVER_STARTUP_FAILED"; break;
			case LOCALDB_ERROR_INSTANCE_CONFIGURATION_CORRUPT: code = "LOCALDB_ERROR_INSTANCE_CONFIGURATION_CORRUPT"; break;
			case LOCALDB_ERROR_CANNOT_CREATE_SQL_PROCESS: code = "LOCALDB_ERROR_CANNOT_CREATE_SQL_PROCESS"; break;
			case LOCALDB_ERROR_UNKNOWN_VERSION: code = "LOCALDB_ERROR_UNKNOWN_VERSION"; break;
			case LOCALDB_ERROR_UNKNOWN_LANGUAGE_ID: code = "LOCALDB_ERROR_UNKNOWN_LANGUAGE_ID"; break;
			case LOCALDB_ERROR_INSTANCE_STOP_FAILED: code = "LOCALDB_ERROR_INSTANCE_STOP_FAILED"; break;
			case LOCALDB_ERROR_UNKNOWN_ERROR_CODE: code = "LOCALDB_ERROR_UNKNOWN_ERROR_CODE"; break;
			case LOCALDB_ERROR_VERSION_REQUESTED_NOT_INSTALLED: code = "LOCALDB_ERROR_VERSION_REQUESTED_NOT_INSTALLED"; break;
			case LOCALDB_ERROR_INSTANCE_BUSY: code = "LOCALDB_ERROR_INSTANCE_BUSY"; break;
			case LOCALDB_ERROR_INVALID_OPERATION: code = "LOCALDB_ERROR_INVALID_OPERATION"; break;
			case LOCALDB_ERROR_INSUFFICIENT_BUFFER: code = "LOCALDB_ERROR_INSUFFICIENT_BUFFER"; break;
			case LOCALDB_ERROR_WAIT_TIMEOUT: code = "LOCALDB_ERROR_WAIT_TIMEOUT"; break;
			case LOCALDB_ERROR_XEVENT_FAILED: code = "LOCALDB_ERROR_XEVENT_FAILED"; break;
			case LOCALDB_ERROR_AUTO_INSTANCE_CREATE_FAILED: code = "LOCALDB_ERROR_AUTO_INSTANCE_CREATE_FAILED"; break;
			case LOCALDB_ERROR_SHARED_NAME_TAKEN: code = "LOCALDB_ERROR_SHARED_NAME_TAKEN"; break;
			case LOCALDB_ERROR_CALLER_IS_NOT_OWNER: code = "LOCALDB_ERROR_CALLER_IS_NOT_OWNER"; break;
			case LOCALDB_ERROR_INVALID_INSTANCE_NAME: code = "LOCALDB_ERROR_INVALID_INSTANCE_NAME"; break;
			case LOCALDB_ERROR_INSTANCE_ALREADY_SHARED: code = "LOCALDB_ERROR_INSTANCE_ALREADY_SHARED"; break;
			case LOCALDB_ERROR_INSTANCE_NOT_SHARED: code = "LOCALDB_ERROR_INSTANCE_NOT_SHARED"; break;
			case LOCALDB_ERROR_ADMIN_RIGHTS_REQUIRED: code = "LOCALDB_ERROR_ADMIN_RIGHTS_REQUIRED"; break;
			case LOCALDB_ERROR_TOO_MANY_SHARED_INSTANCES: code = "LOCALDB_ERROR_TOO_MANY_SHARED_INSTANCES"; break;
			case LOCALDB_ERROR_CANNOT_GET_LOCAL_APP_DATA_PATH: code = "LOCALDB_ERROR_CANNOT_GET_LOCAL_APP_DATA_PATH"; break;
			case LOCALDB_ERROR_CANNOT_LOAD_RESOURCES: code = "LOCALDB_ERROR_CANNOT_LOAD_RESOURCES"; break;
		}

		if (code)
			sprintf(msg, "%s returned %s (0x%x)", method, code, hr);
		else
			sprintf(msg, "%s returned 0x%x", method, hr);

		throwError(Exception::Error(string(msg)));
		return true;
	}
}
