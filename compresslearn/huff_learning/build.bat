@echo off

set CommonCompilerFlags= /W4 /EHsc /O2 /nologo /wd4996
set CommonLinkerFlags= -opt:ref kernel32.lib user32.lib


IF NOT EXIST build mkdir build
pushd build
cl %CommonCompilerFlags% ../main.cpp /link %CommonLinkerFlags% && main
popd 