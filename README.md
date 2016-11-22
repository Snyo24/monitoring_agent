# Maxguage on Cloud Agent
 
## A. Specification

### Supports
* Ubuntu, Debian: 12.04 LTS, 14.04 LTS, 16.04 LTS
* Redhat, CentOS: 6, 7

### Libraries need
* From apt/ yum repositories
    * json-c `sudo apt-get install libjson0-dev`
    * openssl `sudo apt-get install openssl`
    * mysqlclient `sudo apt-get install libmysqlclient-dev`
* Others
    * [zlog](https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz) ([Document](https://hardysimpson.github.io/zlog/UsersGuide-EN.html))
    * [libcurl](https://curl.haxx.se/download/curl-7.50.3.tar.gz) ([Installation guide](https://curl.haxx.se/docs/install.html))

### Source documentation (Need doxygen library) (Not yet ready)

```
$ make doc
```


## B. Install
1. Download tar file

    ```
    $ wget (Download link)
    ```
1. Untar moc_agent.tar.gz

    ```
    $ tar -zxf moc_agent.tar.gz
    ```
1. Write your license in `cfg/licence`
1. Modify your configuration file (See C)
1. Lauch the agent background

    ```
    $ ./bin/agent & 
    [1] (your pid)
    ```

## C. Configuration
    
* Your configuration file is `cfg/plugins`. We support 3 plugins, os, jvm, and mysql.
    * OS plugin
        
        It is written in the conf file by default. You can delete the line to stop observe the OS metrics. OS plugin does not need any option. After you 
        > *os*
    
    * JVM plugin
        
        JVM plugin needs a port to communicate with 
        > jvm,8084
        >
        > mysql,127.0.0.1,root,password

## D. Termination

* If you want to terminate, use this:

        $ kill (your pid)

* Check your agent is terminated

        $ ps aux | grep moc_agent
        [1]+ Terminated    ./bin/moc_agent
