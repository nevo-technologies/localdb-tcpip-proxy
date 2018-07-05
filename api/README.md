# README #

Node wrapper for [localdb api](https://docs.microsoft.com/en-us/previous-versions/sql/sql-server-2012/hh245842(v%3dsql.110))

## Implemented Functions

Refer to a shared instance as `.\foo`

* __listInstanceNames()__
  Return an array of instance names

* __describeInstance__(name)
  Return information about an existing instance or nil:
  ```javascript
  {
    name: string // instance name
    sharedName?: string // if shared
    connectionString?: string // 'np:' {pipeName} of a running or shared instance
    running: boolean,
    automatic: boolean, // an automatic instance
    exists: boolean, // may be false for a `deleted' automatic instance (or maybe never started?)
    corrupted: boolean, // LocalDBInstanceInfo.BOOLbConfigurationCorrupted, whatever that means
    version?: string, // sql server version - major.minor.build.revision
    lastStarted?: number,
    onwerSID?: string
  }
  ```

* __startInstance__(name)
  start an existing instance (may already be running)

* __stopInstance__(name, opts?)
  stop an existing instance (need not be running)
  opts is an optional object with the following optional fields
  ```javascript
  {
    timeout: number, // wait timeout in seconds (default 10) where 0 commences shutdown without awaiting resolution
    noWait: boolean, // SHUTDOWN WITH NOWAIT (default false)
    kill: boolean // kill the sql server process (default false) - takes precendence over noWait
  }
  ```

## TODO

* Implement remaining functions
* Add async versions
* use N-API or NAN for node/v8 formward compat
