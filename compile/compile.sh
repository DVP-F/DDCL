#!/usr/bin/env bash
# This is to compile and run DDCL on Linux using MinGW-w64 and Wine. Requires further testing and such
set -euo pipefail
STARTDIR=$(pwd)
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

C_COMP_OPTS="-std=c++17 -o ../bin/DDCL.exe ../source/app/DDCL.cpp -lpsapi -lws2_32 -lwininet -liphlpapi -static-libgcc -static-libstdc++"

x86_64-w64-mingw32-clang++ $C_COMP_OPTS || x86_64-w64-mingw32-g++ $C_COMP_OPTS || x86_64-w64-mingw32-gcc $C_COMP_OPTS

cd "$SCRIPTDIR/../source/installer"
zig build

cd "$SCRIPTDIR/../source/uninstaller"
zig build

cd "$STARTDIR"

# optional debug run (`DEBUG=1 ./compile.sh`)
if (("$DEBUG" == "1")); then
    export WINEDEUBG=+dll ; wine ../bin/DDCL.exe
fi

