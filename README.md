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

    $ make doc


## B. Install
1. Download tar file

        $ wget (Download link)
    * Check download links here
        * [RHEL 6/ CentOS 6](gadsfadsf)
        * [RHEL 7/ CentOS 7](gadsfadsf)
        * [Debian 7/ Ubuntu 12.04](gadsfadsf)
        * [Debain 8/ Ubuntu 14.04](gadsfadsf)
        * [Ubuntu 16.04](gadsfadsf)
1. Untar moc_agent.tar.gz

        $ tar -zxf moc_agent.tar.gz
1. Write your license in `cfg/licence`
1. Modify your configuration file (See C)
1. Lauch the agent background

    ```
    $ ./bin/agent & 
    [1] (your pid)
    ```

## C. Configuration
    
* Your configuration in in `cfg/plugins`. We support 3 plugins, os, jvm, and mysql.
    * OS
        
        It is written in the conf file by default. You can delete the line to stop observe the OS metrics. OS plugin does not need any option.
        > *os*
    
    * JVM
        
        JVM plugin needs a port to communicate with java agent. Write the port after `jvm,`.
        > *jvm,8084*
    
    * MySQL
        
        MySQL plugin need a database host address, the root account, and its password. Write the options after `mysql,` with seperating comma.
        > *mysql,127.0.0.1,root,password*

## D. Termination

* If you want to terminate, use this:

        $ kill (your pid)

* Check your agent is terminated

        $ ps aux | grep moc_agent
        [1]+ Terminated    ./bin/moc_agent

* If you forgot the agent pid, check with `ps` command.

        $ ps -eo pid | grep moc_agent
        (agent pid)
