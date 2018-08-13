const net = require('net');

class Client {
	constructor(tcp, pipeName) {
		this.closed = Promise.all([
			new Promise(res => this._tcpClosed = res),
			new Promise(res => this._npClosed = res)
		]).then(() => this);

		this._tcp = tcp;
		this._tag = tcp.remotePort; // client `identifier' for logging
		this._np = net.createConnection({path: pipeName});
		this._state = 'open';

		this._tcp.on('error', e => this.onTcpError(e));
		this._tcp.on('close', () => this.onTcpClose());
		this._tcp.pipe(this._np);

		this._np.on('error', e => this.onPipeError(e));
		this._np.on('close', () => this.onPipeClose());
		this._np.pipe(this._tcp);

		console.log('[%s] connected', this._tag);
	}

	stop() {
		if (this.isOpen) {
			this._state = 'stopping';
			this._tcp.setTimeout(1000, () => {
				this._state = 'stopped';
				this._np.end();
				this._tcp.end();
			})
		}
		return this.closed;
	}

	onTcpError(e) {
		this._state = 'tcp-error';
		console.error('[%s] tcp error:', this._tag, e);
		this._np.end();
		// tcp close will fire next
	}

	onTcpClose() {
		let closeType = 'closed';
		if (this.isOpen) {
			closeType = this._state = 'disconnected';
			this._np.end();
		}

		console.log('[%s] %s', this._tag, closeType);
		this._tcpClosed();
	}

	onPipeError(e) {
		this._state = 'pipe-error';
		console.error('[%s] pipe error:', this._tag, e);
		this._tcp.end();
	}

	onPipeClose() {
		this._npClosed();
	}

	get isOpen() { return 'open' === this._state; }
}

module.exports = Client;
