@echo off

cl.exe /O2 /wall /EHsc /std:c++17 /Invapi /Iadl writeValueToDisplay.cpp /link /libpath:nvapi\amd64 /out:writeValueToDisplay.exe


