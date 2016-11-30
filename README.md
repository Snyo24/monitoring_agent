# Maxguage on Cloud Agent
 
## A. Specification

### Supports
* Ubuntu: 12.04 LTS, 14.04 LTS, 16.04 LTS
* Debian: 7, 8
* Redhat, CentOS: 6, 7

### Requirments
* Nothing

### Library used
* From apt/ yum repositories
    * json-c `sudo apt-get install libjson0-dev` `sudo yum install json-c-dev`
    * openssl `sudo apt-get install libssl-dev` `sudo yum install openssl-dev`
    * mysqlclient `sudo apt-get install libmysqlclient-dev` `sudo yum install mysql-dev`
    * curl `sudo apt-get install libcurl4-openssl-dev`
* Others
    * [zlog](https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz) ([Document](https://hardysimpson.github.io/zlog/UsersGuide-EN.html)) (Do not have to install, included)

## B. Install
1. Download tar file

        $ wget (Download link)
    * Check download links here
        * [RHEL 6](gadsfadsf)
        * [RHEL 7](gadsfadsf)
        * [CentOS 6](gadsfadsf)
        * [CentOS 7](gadsfadsf)
        * [Debian 7](gadsfadsf)
        * [Debain 8](https://s3.ap-northeast-2.amazonaws.com/maxgaugeoncloud/downloads/agent/debian_8.6/moc_agent_debian_8.6_v1.0.tar.gz)
        * [Ubuntu 12.04](gadsfadsf)
        * [Ubuntu 14.04](https://s3.ap-northeast-2.amazonaws.com/maxgaugeoncloud/downloads/agent/ubuntu_14.04/moc_agent_ubuntu_14.04_v1.0.tar.gz)
        * [Ubuntu 16.04](gadsfadsf)
1. Untar moc_agent.tar.gz

        $ tar -zxf moc_agent.tar.gz
1. Write your license in `cfg/license`
1. Modify your configuration file (See [C. Configuration](#c-configuration))
1. Lauch the agent background

    ```
    $ ./bin/agent & 
    [1] (agent pid)
    ```

## C. Configuration
    
* Your configuration is in `cfg/plugins`. We support 3 plugins, os, jvm, and mysql. Add the configuration by following instructions.
    * **OS**
        
        It is written in the conf file by default.
        You can delete the line to stop observe the OS metrics.
        OS plugin does not need any option.

        In `cfg/plugins`, add a line `os`.
        > os
    
    * **JVM**
        
        JVM plugin needs a port to communicate with java agent. We call it jspd.
        The jspd runs with tomcat to collect metrics from tomcat.
        So, we must set the java agent to run with tomcat at the same time.
        
        At the first line of `$(tomcat path)/bin/catalina.sh`,
        
        > export JSPD_HOME=(your agent path)/jspd
        >
        > export JAVA_OPTS="-javaagent:$JSPD_HOME/lib/jspd.jar -noverify"
        
        In `cfg/plugins`, add a line `jvm`.
        > jvm
    
    * **MySQL**
        
        MySQL plugin needs a host address, the root account, and its password. Write the options after `mysql` with seperating commas.
        > mysql,127.0.0.1,root,password

## D. Termination

* If you want to terminate, use this:

        $ kill (agent pid)

* Check your agent is terminated

        $ ps aux | grep moc_agent
        (print nothing)

* If you forgot the agent pid, check `res/pid`.

        $ cat res/pid
        (agent pid)
