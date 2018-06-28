// addon is named 'localdb_api' as 'localdb-api' doesnt compile - rename it
const api = require('bindings')('localdb_api');

module.exports = api;
