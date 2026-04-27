# DDCL - DiskDriveConnectionLogger  

DDCL is a simple network and drive connection logger for scamdows designed to be lighweight, portable, and easy to use.  
It is written in C\++17 and uses the toml++ library for configuration management.  
It's also one of __very__ few C++ application in my career, so dont mind the bad code lol.  

## ToC  

- [Header](#ddcl---diskdriveconnectionlogger)  
  - [Credits](#credits)  
    - [Toml++](#toml---a-c17-library-for-toml-parsing-and-serialization)  
    - [Everything else](#everything-else)  
  - [Performance and statistics](#performance-and-statistics)  
  - [Files](#files)  
    - [Binaries](#binaries)  
    - [Scripts and Source code](#scripts-and-source-code)  
    - [Licenses](#licenses)  
    - [Other](#other-files)  
  - [How2](#how-to-use)  
    - [Installation Packages](#installation-packages)  
      - [Compilation](#compilation)  
      - [Recommended Environment](#recommended-environment)  
      - [Requirements for Installation](#requirements-for-installation)  
    - [Archive Sets](#archive-sets)  
      - [Full](#full-set-ddcl_fullzip)  
      - [Manual](#manual-set-ddcl_manualzip)  
      - [Standalone](#standalone-set-ddcl_standalonezip)  
    - [Standalone EXE](#standalone-executable-use)  
    - [Full Install](#full-install)  
    - [Logging](#logging)  
      - [Log Format](#log-format)  
    - [Commandline](#commandline-options-for-ddclexe)  

## Credits  

See `NOTICE.txt`

### toml++ - a C++17 library for TOML parsing and serialization  

toml.hpp : [https://github.com/marzer/tomlplusplus/blob/v3.4.0/toml.hpp](https://github.com/marzer/tomlplusplus/blob/v3.4.0/toml.hpp)  
Copyright: Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>  
License: `LICENSES/LICENSE.mit` [(MIT)](https://mit-license.org/)  

### Everything else  

Me :D  
Links to hosts:  
[Github](https://github.com/DVP-F/DDCL)  
Copyright: Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>  
License: `LICENSES/LICENSE.gpl3` [(GPL-3.0)](https://www.gnu.org/licenses/gpl-3.0.html)  

## Performance and statistics  

Test results can be found in [PERFORMANCE.md](./PERFORMANCE.md)  
Statistics i have yet to produce.  

## Files  

Assuming default names.

### Binaries  

- DDCL.exe
  - The actual logging binary, the interface for monitoring.
  - Compiled from `DDCL.cpp`
- DDCL_Installer.exe
  - This registers the runner binary `DDCL.exe`, registry keys, and %PATH% for ease of use.
  - Compiled from `installer.py`
- DDCL-Setup.msi
  - An installer package. This copies over the necessary files (including a customized `conf.toml`)  
    and then executes `DDCL_Installer.exe`.
  - Compiled from `DDCL.exe`, `DDCL_Installer.exe`, `conf.toml`, and temporary files.

### Scripts and source code  

- DDCL.cpp
  - The main source, the actuall UI and logger.
- toml.hpp
  - The C++17 TOML parsing library used by DDCL.
- compile_package.bat
  - This is the primary compilation script, which handles python scripts and the runner,  
    as well as MSI packaging.
- installer.bat
  - The secondary registration script - registers the runner binary `DDCL.exe`, registry keys, and %PATH%
- compile.sh
  - Compilation script for DDCL.exe when cross-compiling from Linux

### Licenses  

- LICENSE.gpl3
  - This project's license - GPL-3.0
- LICENSE.mit
  - toml.hpp license - MIT

### Documents  

- [./](./) - Root directory  
  - [README.md](./README.md)  
    - Repository README, this file.  
  - [TODO.md](./TODO.md)  
    - TODO list, changes to be made, plans.  
  - [NOTICE.txt](./NOTICE.txt)
    - License and copyright notice  
    - Source repository  
- [./docs/](./docs/) - Technical documentation  
  - [compile_package.bat.md](./docs/compile_package.bat.md)  
    - Technical documentation of [compile_package.bat](./compile/compile_package.bat)  
  - [DDCL.cpp.md](./docs/DDCL.cpp.md)  
    - Technical documentation of [DDCL.cpp](./source/DDCL.cpp)  
  - [installer.bat.md](./docs/installer.bat.md)  
    - Technical documentation of [installer.bat](./source/installer.bat)  
  - [installer.py.md](./docs/installer.py.md)  
    - Technical documentation of [installer.py](./source/installer.py)  

### Other files  

- [./source/conf.toml](./source/conf.toml)
  - Depending on your installation, this will either be the default configuration or a customized one.
  This file is used by DDCL to store what, if, and how it should test things.  
- [./bin/conf.toml](./bin/conf.toml)
  - Configuration file for Wine when cross-compiling from Linux
- [./compile/.clangd](./compile/.clangd)
  - Configuration file for CLang when cross-compiling from Linux

## How to use  

### Installation packages

You can use a precompiled package from the repository or compile from source.  
Compiling from source is easiest done with a modified `compile/compile_package.bat`  
This will give you the executables for the runner, installer, lms, and a `.msi` package installer.  

If you do wanna compile with commands manually, all the info you need should be in the same file.  

If compiling from source, prior to compilation, remember to:  

- modify the `source/conf.toml` TOML configuration file as it's copied wholesale into the MSI  
- edit the environment variables at the top of `compile/compile_package.bat` to match your use.
- edit the values used in `source/installer.py` (`_add_lnk()`, `_find_installation_path()`) to match your use

#### Compilation  

Minimum recommended is lowest tested version.  
Minimum version is lowest version with known compatible syntax.  

- MSVC CLang (minimum recommended: MSVC 14.44.35207)
- MSVC Linker (minimum recommended: MSVC 14.44.35207)
- MS Resource Compiler (minimum recommended: Windows Kits for 10.0.26100.0)
- WiX CLI Tools (minimum version: 7.0)
- Python (minimum version: 3.10.x)
- Python packages:
  - `pyuac` (administrator elevation)
  - `pywin32` or `pypiwin32` (for pyuac and win32com)
  - `nuitka` (binary compilation)
  - `zstandard` (compression) 

About PyWin32 and PyPiWin32 :  

- [mail.python.org, 27.10.2016](https://mail.python.org/pipermail/python-win32/2016-October/013786.html)  
- [PyPi, PyWin32](https://pypi.org/project/pywin32/)  
- [GitHub, brandond/requests-negotiate-sspi Issue 13](https://github.com/brandond/requests-negotiate-sspi/issues/13)  

#### Recommended environment  

- MS Visual Studio 2022 CE with C++ workload
- WiX CLI Tools 7.0
- Python 3.12+ with `pyuac`, `pywin32`, `nuitka`, `zstandard`

#### Requirements for installation  

Windows 10/11  
Local administrator access  

### Archive sets

These are the .zip archives made by `compile/compile_package.bat`  
These are generated under `dist/`  

Each additionally contains License and Copyright notices by way of these files:  

- [./LICENSES/LICENSE.gpl3](./LICENSES/LICENSE.gpl3)  
- [./LICENSES/LICENSE.mit](./LICENSES/LICENSE.mit)  
- [./NOTICE.txt](./NOTICE.txt)  

#### Full set (`DDCL_full.zip`)  

Contains the following:  

1. `DDCL-Setup.msi`
2. `DDCL.exe`
3. `DDCL_Installer.exe`
4. `conf.toml`

#### Manual set (`DDCL_manual.zip`)  

Contains the following:  

1. `DDCL.exe`
2. `installer.bat`
3. `conf.toml`

#### Standalone set (`DDCL_standalone.zip`)  

Contains the following:  

1. `DDCL.exe`
2. `conf.toml`

### Standalone executable use

1. Get an installation package
2. Move the `DDCL.exe` binary to the desired location.
3. Run it once to generate the default configuration file (config.toml) in the same directory.
4. Edit the config file with what disks to check for, IP address to check connection to,  
   UNC (SMB, etc.) paths to verify, DNS ovverride, expected DNS suffix, and more.
5. Run it again to start logging!

### Full install

1. Get an installation package
2. Choose one of two paths - Manual managed install or MSI install.  
   
   For Manual install:  
   
   1. Move the `DDCL.exe` binary to the desired location.  
   2. Run it once to generate the default configuration file (config.toml) in the same directory.  
   3. Edit the config file with what disks to check for, IP address to check connection to,    
      UNC (SMB, etc.) paths to verify, DNS ovverride, expected DNS suffix, and more.
   4. Run either `register_service.bat` or `DDCL_Installer.exe` to register the services.  

   For MSI install:  

   1. Run `DDCL-Setup.msi`  
   The MSI should copy the files into either `C:\Program Files\DDCL` or `C:\Program Files (x86)\DDCL`  
   \- and run `DDCL_Installer.exe`.
3. This should register the install location on %PATH%, so you can call DDCL merely by name in any terminal.

### Logging  

Logs are written to one of the following locations, in the same prioritization  
(Edit the code if you want it to be different, dont be cheap. Learn some C++):  

- {log_path if defined in `conf.toml`}\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv
- %LOCALAPPDATA%\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv
- %OneDriveCommercial%\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv
- %OneDrive%\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv
- C:\Users\%USERNAME%\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv
- &EXE_DIR&\DDCL-Logs\DDCL_log-{´%d.%m.%Y-%H_%M_%S´}.csv

The timestamp is the time of launching the application.  

The log contains only the CHANGES in status and the initial state, with a timestamp and any other relevant information.

#### Log format  

The log is in CSV format (comma delimited ","), with the following columns:  

- `timestamp`: The date and time of the log entry.  
- `kind`: The type of the log entry, self descriptive.  
- `value`: The value of the log entry, self descriptive.  
- `info`: Any additional information about the log entry.  

Example:  

```csv
timestamp,kind,value,info
01.04.2032-13:47:40,internet_connectivity,online
01.04.2032-13:47:40,ethernet,Ethernet 8;example.domain.tld,initial_status
01.04.2032-13:47:40,dns_resolution,1.1.1.1:sys_dns;True 1.1.1.1:{given_dns};True www.wikipedia.org:sys_dns;False www.wikipedia.org:{given_dns};True
01.04.2032-13:47:40,vpn_connection,not_connected l_ip:N/A
01.04.2032-13:47:40,drive_availability,available,C
01.04.2032-13:47:40,unc_availability,available,\\redacted_path\somewhere
```

### Commandline options for DDCL.exe  

- -c, --config  
   Print the current configuration to the console and exit.   
