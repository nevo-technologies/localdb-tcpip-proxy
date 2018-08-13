const net = require('net');
const api = require('localdb-api');
const Client = require('./client');

const NP = /^np:/i;

class Server {
	constructor(opts) {
		this._opts = opts;
		this._tcp = net.createServer(x => this.onConnection(x));
		this._clients = []; // Client[]
		this._sql = null; // InstanceInfo
		this._stopSql = false;
	}

	get clientCount() { return this._clients.length; }

	async start() {
		this.initSql();
		try {
			await this.listen();
			return this;
		}
		catch (e) {
			this.stopSql();
			throw e;
		}
	}

	async stop() {
		await this.stopTcp();
		this.stopSql();
	}

	get pipeName() {
		let cs = this._sql && this._sql.connectionString;
		return cs && cs.replace(NP, '');
	}

	initSql() {
		let name = this._opts.instance;
		let sql = this._sql = api.describeInstance(name);
		if (!sql) throw `unknown instance: ${name}`;
		if (sql.running) {
			console.log('%s running on pipe: %s', name, this.pipeName);
			return;
		}
		if (!this._opts.autoStart) throw `instance not running: ${name}`;

		console.log('starting instance %s...', name);
		sql.connectionString = api.startInstance(name);
		this._stopSql = true;
		console.log('%s started on pipe: %s', name, this.pipeName);
	}

	stopSql() {
		if (!this._stopSql) return;
		this._stopSql = false;
		let name = this._opts.instance;
		console.log('stopping instance %s...', name);
		api.stopInstance(name, {timeout: this._opts.stopSqlTimeout});
	}

	listen() {
		let port = this._opts.port;
		return new Promise((res, rej) => {
			this._tcp.on('error', rej);
			this._tcp.listen(port, () => {
				console.log('tcp listening on %d', port);
				res(this);
			});
		});
	}

	onConnection(tcp) {
		let client = new Client(tcp, this.pipeName);
		this._clients.push(client);
		client.closed.then(x => {
			let off = this._clients.indexOf(x);
			off > -1 && this._clients.splice(off, 1);
		});
	}

	async stopTcp() {
		// stop accepting connections
		let closed = [new Promise(res => this._tcp.close(res))];
		// close open connections
		this._clients.forEach(x => closed.push(x.stop()));

		await Promise.all(closed);
		console.log('tcp close');
	}
}

module.exports = Server;
