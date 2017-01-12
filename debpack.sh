#!/bin/bash
# This script is used to make a .dep package.
# This script should be executed in agent_factory directory.
# If not working, modify first line to "#!/bin/sh".

# Build agent
make clean
make all

# Make new file archive
rm -rf maxgaugeair_deb
mkdir -p maxgaugeair_deb/DEBIAN
mkdir -p maxgaugeair_deb/usr/bin
mkdir -p maxgaugeair_deb/usr/lib
mkdir -p maxgaugeair_deb/etc/maxgaugeair/config
mkdir -p maxgaugeair_deb/etc/maxgaugeair/etc
mkdir -p maxgaugeair_deb/etc/maxgaugeair/log

# Copy agent binary, configuration files
cp bin/agent        maxgaugeair_deb/usr/bin/maxgaugeair-agent
cp cfg/license      maxgaugeair_deb/etc/maxgaugeair/config/license
cp cfg/plugins      maxgaugeair_deb/etc/maxgaugeair/config/plugins
cp cfg/.zlog.conf   maxgaugeair_deb/etc/maxgaugeair/.zlog.conf
cp cfg/plugin.conf  maxgaugeair_deb/etc/maxgaugeair/plugin.conf

# Copy library files
cp -r lib/* maxgaugeair_deb/usr/lib/

# Copy spec file, scripts and daemon configurations
cp -r pkg_files/deb/* maxgaugeair_deb/

# Finally pack the file archive using dpkg
dpkg -b maxgaugeair_deb maxgaugeair_agent.deb

# Remove file archive, builds
rm -rf maxgaugeair_deb
make clean
