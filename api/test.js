const api = require('bindings')('localdb_api');

let versions = api.listVersions();
console.log('installed versions:', versions.join(', '));

let names = api.listInstanceNames();
for (let name of names) {
	let info = api.describeInstance(name);
	console.log('[%s]', name, info);
}
