let pgm = require('commander');

const Server = require('./lib/server');

pgm
	.option('-i, --instance <name>', 'instance name')
	.option('-p, --port [port]', 'tcp port', toInt, 1433)
	.option('-a, --auto-start', 'start instance')
	.option('-t, --stop-sql-timeout [seconds]', 'stop instance timeout [10] (0 initiates shutdown without waiting)', toInt, 10)
;

pgm.parse(process.argv);
if (!(pgm.instance && pgm.port > 0 && pgm.stopSqlTimeout > -1))
	pgm.help();

let server = new Server(pgm.opts());
server.start()
	.then(() => {
		process.on('SIGINT', () => {
			let clients = server.clientCount;
			if (clients)
				console.log('attempting to close %d open connection(s) - make take a while...', clients);
			server.stop();
		});
	}, bail)
;

function bail(e) {
	console.error(e);
	process.exit(1);
}

function toInt(s) {
	n = Number(s);
	return Number.isInteger(n) ? n : NaN;
}
