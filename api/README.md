# README #

Node wrapper for [localdb api](https://docs.microsoft.com/en-us/previous-versions/sql/sql-server-2012/hh245693(v%3dsql.110))

## Functions

Refer to a shared instance as `.\foo`

* describeInstance(name)
  Returns information about an existing instance or nil
  ```javascript
  {
  	name: string // instance name
  	sharedName?: string // if shared
  	connectionString?: string // 'np:' {pipe} of a shared or running instance
  	running: boolean,
  	automatic: boolean, // an automatic instance
  	corrupted: boolean, // LocalDBInstanceInfo.BOOLbConfigurationCorrupted, whatever that means
  	version?: string, // sql server version, ie major.minor.build.revision
  	lastStarted?: number,
  	onwerSID?: string
  }
  ```

* startInstance(name)
  start an existing instance (may already be running)

* stopInstance(name)
  stop an existing instance (may already be stopped)

## TODO

* Implement remaining functions
* Add async versions
* use N-API or NAN for node/v8 formward compat
