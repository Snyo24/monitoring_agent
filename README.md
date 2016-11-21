# INSTALLATION GUIDE
 
##1. Specification

### Environment
- Ubuntu 64bit (Linux)
- Language: C

### Libraries need
- zlog (https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz)
- libcurl (https://curl.haxx.se/download/curl-7.50.3.tar.gz)
- json-c (sudo apt-get install libjson0-dev)
- openssl-dev
- libmysqlclient-dev

### Generate documents (Need doxygen library) (Not supported yet)
    $ make doc

## 2. Install process
#####a. Download tar file
    wget address
#####b. get moc_agent.tar.gz
    tar -zxf moc_agent.tar.gz
#####c. Install the libraries listed above
#####d. Modify configuration files
Add lines of plugins that you want to observe, one of "os", "proxy", "mysql". A following space is needed after a line.

    vim cfg/plugins

> os, 

> proxy, 

> mysql, 

Modify the license file.

    vim cfg/license

#####e. Compile and Lauch!
    make
    ./bin/agent & 
    [1] *pid*

#####f. If you want to terminate, use this
    kill *pid*
