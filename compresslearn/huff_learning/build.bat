@echo off

set CommonLinkerFlags= -opt:ref kernel32.lib user32.lib


IF NOT EXIST build mkdir build
pushd build
cl /W4 /O2 /nologo /EHsc ../main.cpp /link %CommonLinkerFlags% && main
popd 