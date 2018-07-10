// https://gonzalo123.com/2012/09/17/building-a-simple-tcp-proxy-server-with-node-js/

const net = require('net');
const parseArgs = require('minimist');
const api = require('localdb-api');

const NP = /^np:/i;

let args = parseArgs(process.argv.slice(2), {
	alias: {
		name: ['n'],
		port: ['p'],
		autoStart: ['a', 'auto-start']
	}
});
let {name, port = 1433} = args;

if (!name)
	bail('missing name');
if (!((port = Number(port)) > 0 && Number.isInteger(port)))
	bail('invalid port:', args.port);

let info = api.describeInstance(name);
let pipe;
let stopSql = false;
let stopping = false;
if (!info)
	bail('No such instance: %s', name);
if (info.running)
	pipe = info.connectionString.replace(NP, '');
else if (!args.autoStart)
	bail('%s is not running', info.name);
else {
	console.log('Starting %s...', info.name);
	pipe = api
		.startInstance(name) // throws on error
		.replace(NP, '')
	;
	stopSql = true;
}
console.log('%s running on pipe: %s', info.name, pipe);

let clients = []; // for shutdown
let server = net.createServer((tcp) => {
	let tag = tcp.remotePort;
	console.log('[%d] client connected', tag);
	clients.push(tcp);

	let np = net.createConnection({path: pipe});
	tcp.on('data', (x) => np.write(x));
	tcp.on('error', (e) => console.error('[%d]:', tag, e));
	tcp.on('close', (had_error) => {
		console.log('[%d] client %s', tag, stopping ? 'destroyed' : 'disconnected');

		let off = clients.indexOf(tcp);
		off > -1 && clients.splice(off, 1);
		np.end();
	});

	np.on('data', (x) => tcp.write(x));
	np.on('error', (e) => {
		if (!(stopSql && stopping && 'EPIPE' === e.code))
			console.error('[%d] pipe error:', tag, e);
	});
});
server.on('close', () => {
	console.log('tcp close');

	if (!stopSql) return;
	console.log('Stopping %s...', info.name);
	api.stopInstance(info.name, {timeout: 30}); // TODO - timeout from args
});

server.listen(port, () => {
	console.log('tcp listening on %d', port);
});

process.on('SIGINT', () => {
	stopping = true;
	server.close();
	clients.slice().forEach((x) => x.destroy()); // end takes longer
});

function bail(...args) {
	args.length && console.error(...args);
	process.exit(1);
}