#!/bin/bash
# This script is used to make a .dep package.
# This script should be executed in agent_factory directory.
# If not working, modify first line to "#!/bin/sh".

# Build agent
make clean
make all

# Make new file archive
rm -rf moc_deb
mkdir -p moc_deb/DEBIAN
mkdir -p moc_deb/usr/bin
mkdir -p moc_deb/usr/lib
mkdir -p moc_deb/etc/moc/config
mkdir -p moc_deb/etc/moc/etc

# Copy agent binary, configuration files
cp bin/agent moc_deb/usr/bin/moc-agent
cp cfg/license moc_deb/etc/moc/config/license
cp cfg/plugins moc_deb/etc/moc/config/plugins
cp .aid moc_deb/etc/moc/etc/.aid

# Copy library files
cp -r lib/* moc_deb/usr/lib/

# Copy spec file, scripts and daemon configurations
cp -r pkg_files/deb/* moc_deb/

# Finally pack the file archive using dpkg
dpkg -b moc_deb moc_agent.deb

# Remove file archive, builds
rm -rf moc_deb
make clean
