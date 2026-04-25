#!/usr/bin/env bash
# This is to compile and run DDCL on Linux using MinGW-w64 and Wine. Requires further testing and such
set -euo pipefail

# define help message
help_print() {
    cat <<EOF
Usage:
$0 [-h|--help] <Compiler> <Package manager>

-h, --help
    Print this help message
<Compiler>:
    gcc, g++, cc, clang
<Package manager>:
    apt, apt-get, pacman

Exit codes:
1 - Unexpected error
2 - Help message
3 - Incorrect syntax
    (Missing/unknown arguments)

Check exit codes with:
\`echo \$?\`
\`echo \$PIPESTATUS\` (bash exclusive)
EOF
    exit 2
}

# Sanitize arguments and deal with help arg
handle_args() {
    # idc enough to use getopt, sue me
    if [[ $# -gt 2 && $# -eq 0 ]]; then 
        echo "Error: expected 1 (one) to 2 (two) arguments." >&2
        help_print
    fi
    case "$1" in 
        -h|--help)
            help_print
            ;;
        gcc|g++|cc|clang)
            echo "Using compiler '$1'"
            ;;
        *)
            echo "Unknown compiler: '$1'" >&2
            exit 3
            ;;
    esac
    if [[ $# -eq 2 ]]; then
        case "$2" in
            apt|apt-get|pacman)
                echo "Using package manager '$2'"
                ;;
            *)
                echo "Unknown package manager: '$2'" >&2
                exit 3
                ;;
        esac
    else
        echo "No package manager given; Aborting" >&2
        exit 3
    fi
}

# functions to install mingw if not installed
install_mingw_gcc() {
    echo "gcc/cc equivalent Mingw64 not found, attempting install."
    if [[ $# -eq 2 ]]; then
        case "$2" in
            apt|apt-get)
                if dpkg -s gcc-mingw-w64 >/dev/null 2>&1; then
                    echo "gcc-mingw-w64 already installed"
                else
                    sudo "$2" update && sudo "$2" install -y gcc-mingw-w64 || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
            pacman)
                if pacman -Qi mingw-w64-x86_64-gcc >/dev/null 2>&1; then
                    echo "mingw-w64-gcc already installed"
                else
                    pacman -Sy && pacman -S --noconfirm mingw-w64-x86_64-gcc || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
        esac
    else
        echo "No package manager given, aborting." >&2
        exit 4
    fi
}
install_mingw_gpp() {
    echo "g++ equivalent Mingw64 not found, attempting install."
    if [[ $# -eq 2 ]]; then
        case "$2" in
            apt|apt-get)
                if dpkg -s g++-mingw-w64 >/dev/null 2>&1; then
                    echo "g++-mingw-w64 already installed"
                else
                    sudo "$2" update && sudo "$2" install -y g++-mingw-w64 || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
            pacman)
                if pacman -Qi mingw-w64-x86_64-gcc >/dev/null 2>&1; then
                    echo "mingw-w64-x86_64-gcc already installed"
                else
                    pacman -Sy && pacman -S --noconfirm mingw-w64-x86_64-gcc || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
        esac
    else
        echo "No package manager given, aborting." >&2
        exit 4
    fi
}
install_mingw_clang() {
    echo "clang equivalent Mingw64 not found, attempting install."
    if [[ $# -eq 2 ]]; then
        case "$2" in
            apt|apt-get)
                if dpkg -s clang-mingw-w64 >/dev/null 2>&1; then
                    echo "clang-mingw-w64 already installed"
                else
                    sudo "$2" update && sudo "$2" install -y clang-mingw-w64 || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
            pacman)
                if pacman -Qi mingw-w64-clang-x86_64-toolchain >/dev/null 2>&1; then
                    echo "mingw-w64-clang-x86_64-toolchain already installed"
                else
                    pacman -Sy && pacman -S --noconfirm mingw-w64-clang-x86_64-toolchain || {
                        echo "Failed to install with $2!" >&2
                        exit 1
                    }
                fi
                ;;
        esac
    else
        echo "No package manager given, aborting." >&2
        exit 4
    fi
}

# define c compiler functions
compile_gcc() {
    x86_64-w64-mingw32-gcc -std=c++17 -o ../bin/DDCL.exe ../source/DDCL.cpp -lpsapi -lws2_32 -lwininet -liphlpapi -static-libgcc -static-libstdc++
}
compile_gpp() {
    x86_64-w64-mingw32-g++ -std=c++17 -o ../bin/DDCL.exe ../source/DDCL.cpp -lpsapi -lws2_32 -lwininet -liphlpapi -static-libgcc -static-libstdc++
}
compile_clang() {
    x86_64-w64-mingw32-clang++ -std=c++17 -o ../bin/DDCL.exe ../source/DDCL.cpp -lpsapi -lws2_32 -lwininet -liphlpapi -static-libgcc -static-libstdc++
}

try_compile() {
    # check that compiler exists and is available
    # if it is, compile
    # if it isnt, try to install it.
    case "$1" in
        gcc|cc)
            if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
                compile_gcc
            # if not, check with pkgman as well as if it can be installed (if defined)
            else
                install_mingw_gcc && compile_gcc
            fi
            ;;
        g++)
            if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
                compile_gpp
            # if not, check with pkgman as well as if it can be installed (if defined)
            else
                install_mingw_gpp && compile_gpp
            fi
            ;;
        clang)
            if command -v x86_64-w64-mingw32-clang++ >/dev/null 2>&1; then
                compile_clang
            # if not, check with pkgman as well as if it can be installed (if defined)
            else
                install_mingw_clang && compile_clang
            fi
            ;;
        # Already sanitized the contents of $1 so no default here
    esac
}

# pass all args along to the handlers
handle_args $@
try_compile $@

# export WINEDEUBG=+dll ; wine ../bin/DDCL.exe
