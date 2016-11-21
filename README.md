# Maxguage on Cloud Agent
 
## A. Specification

### Supports
* Ubuntu, Debian: 12.04 LTS, 14.04 LTS, 16.04 LTS
* Redhat, CentOS: 6, 7

### Libraries need
* [zlog](https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz)
* [libcurl](https://curl.haxx.se/download/curl-7.50.3.tar.gz)
* json-c (libjson0-dev)
* openssl (openssl)
* mysqlclient (libmysqlclient-dev)

### Source documentation (Need doxygen library) (Not yet ready)
```
$ make doc
```

<<<<<<< HEAD
## 2. Install process
##### a. Download tar file
    $ wget (address)
##### b. Get moc_agent.tar.gz
    $ tar -zxf moc_agent.tar.gz
##### c. Install the libraries listed above
##### d. Modify configuration files
Add lines of plugins that you want to observe, one of "os", "proxy", "mysql". A following space is needed after a line.

    $ vim cfg/plugins
=======
## B. Install
1. Download tar file

    ```
    $ wget (Download link)
    ```
1. Untar moc_agent.tar.gz

    ```
    $ tar -zxf moc_agent.tar.gz
    ```
1. Install the libraries listed above
1. Modify configuration files

    Add lines of plugins that you want to observe, one of "os", "proxy", "mysql". A following space is needed after a line

        $ vim cfg/plugins

    > 
    > os
    >
    > jvm,8084
    >
    > mysql,127.0.0.1,root,password

    $ vim cfg/license

    Modify the license file.

        $ vim cfg/license
1. Lauch!

    ```
    $ ./bin/agent & 
    [1] pid
    ```

## C. Termination
If you want to terminate, use this

    $ kill pid
