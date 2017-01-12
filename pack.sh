cd ..
mkdir -p moc_agent
cp -ra agent_factory/. moc_agent/

cd moc_agent
rm -rf .git/ .gitignore inc/ obj/*.o src/ Makefile .uuid .pid log/* pack.sh obj/ Doxyfile* .*.swp
cd ..
tar -zcf moc_agent.tar.gz moc_agent
