# This is to compile and run DDCL on Linux using MinGW-w64 and Wine. Requires further testing and such

x86_64-w64-mingw32-gcc -std=c++17 -o ../bin/DDCL.exe ../source/DDCL.cpp \
-lpsapi -lws2_32 -lwininet -liphlpapi -lstdc++ -static
export WINEDEUBG=+dll ; wine ../bin/DDCL.exe
