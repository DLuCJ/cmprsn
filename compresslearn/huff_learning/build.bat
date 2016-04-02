@echo off

IF NOT EXIST build mkdir build
pushd build
cl /W4 /O2 /nologo /EHsc ../main.cpp && main
popd 