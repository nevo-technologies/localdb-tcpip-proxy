const api = require('bindings')('localdb_api');

let versions = api.listVersions();
for (let version of versions) {
	let info = api.describeVersion(version);
	console.log('[%s]', version, info);
}

let names = api.listInstanceNames();
for (let name of names) {
	let info = api.describeInstance(name);
	console.log('[%s]', name, info);
}
