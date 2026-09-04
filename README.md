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
  - [How To](#how-to-use)  
    - [Installation Packages](#installation-packages)  
      - [Version Requirements](#version-requirements)  
      - [Recommended Environment](#recommended-environment)  
      - [Requirements for Installation](#requirements-for-installation)  
    - [Archive Sets](#archive-sets)  
      - [Full](#full-set-ddcl_fullzip)  
      - [Manual](#manual-set-ddcl_manualzip)  
      - [Standalone](#standalone-set-ddcl_standalonezip)  
    - [Standalone EXE](#standalone-executable-use)  
    - [Full Install](#full-install)  
    - [Running the app](#running)
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

Test results can be found in [docs/data/PERFORMANCE.md](./docs/data/PERFORMANCE.md)  
Statistics i have yet to produce.  

The data files will also be under [docs/data/](./docs/data/).

## Files  

Assuming default names.

### Binaries  

- DDCL.exe
  - The actual logging binary, the interface for monitoring.
  - Compiled from `DDCL.cpp`
- installer.exe
  - This registers the registry keys, and modifies %PATH% for ease of use.  
- uninstaller.exe
  - This removes the registry keys and %PATH% modification.  
- DDCL-Setup.msi
  - An installer package. This copies over the necessary files (including a customized `conf.toml`)  
    and then executes `installer.exe`.
  - Contains the other binaries, licenses, `conf.toml`, and `NOTICE.txt`

### Scripts and source code  

- [./source/](./source/)  
  - [app/](./source/app/)  
    - [DDCL.cpp](./source/app/DDCL.cpp)  
      - The main source, the actual UI and logger.  
    - [toml.hpp](./source/app/toml.hpp)  
      - The C++17 TOML parsing library used by DDCL.  
  - [installer/](./source/installer/)  
    - [src/main.zig](./source/installer/src/main.zig)  
      - The installer's main source code.  
  - [uninstaller/](./source/uninstaller/)  
    - [src/main.zig](./source/uninstaller/src/main.zig)  
      - The uninstaller's main source code.  
- [./compile/](./compile/)  
  - [compile_package.bat](./compile/compile_package.bat)  
    - This is the primary compilation script, which handles compiling the binaries and creating zip archives.  
  - [compile.sh](./compile/compile.sh)  
    - Compilation script for DDCL.exe when cross-compiling from Linux.  

### [Licenses](./LICENSES/)  

- [LICENSE.gpl3](./LICENSES/LICENSE.gpl3)
  - This project's license - GPL-3.0
- [LICENSE.mit](./LICENSES/LICENSE.mit)
  - toml.hpp license - MIT

### Documents  

- [./](./) - Root directory  
  - [README.md](./README.md)  
    - Repository README, this file.  
  - [TODO.md](./TODO.md)  
    - TODO list, changes to be made, plans.  
  - [NOTICE.txt](./NOTICE.txt)
    - License and copyright notice  
    - Source repository link  
- [./docs/](./docs/) - Documentation  
  - [/data/](./docs/data/) - Data sets  
    - [PERFORMANCE.md](./docs/data/PERFORMANCE.md)  
      - Documentation of performed tests  
    - Various other data  

### Other files  

- [./source/app/conf.toml](./source/app/conf.toml)
  - Depending on your installation, this will either be the default configuration or a customized one.
  This file is used by DDCL to store what, if, and how it should test things.  
- [./bin/conf.toml](./bin/conf.toml)
  - Configuration file for Wine when cross-compiling from Linux
- [./compile/.clangd](./compile/.clangd)
  - Configuration file for CLang when cross-compiling from Linux
  A copy of this exists at [./source/app/.clangd](./source/app/.clangd)  

## How to use  

### Installation packages

You can use a precompiled package from the repository or compile from source.  
Compiling from source is easiest done with a modified `compile/compile_package.bat`  

If you do wanna compile with commands manually, all the info you need should be in the same file.  

If compiling from source, prior to compilation, remember to:  

- modify the `source/conf.toml` TOML configuration file as it's copied wholesale  
- edit the environment variables at the top of `compile/compile_package.bat` to match your use.

The equivalent actions ought to be applicable to `compile/compile.sh`

#### Version requirements  

Minimum recommended is lowest tested version.  
Minimum version is lowest version with known compatible syntax.  
Required version is the only applicable version.  

- MSVC CLang (minimum recommended: MSVC 14.44.35207)
- MSVC Linker (minimum recommended: MSVC 14.44.35207)
- MS Resource Compiler (minimum recommended: Windows Kits for 10.0.26100.0)
- WiX CLI Tools (minimum version: 4.0)
- Zig (required version: 0.15.2)

Recommended environment:  

- MS Visual Studio 2022 CE with C++ workload
- WiX CLI Tools 7.0
- Zig 0.15.2

#### Requirements for installation  

Windows 10/11  
Local administrator access  

### Archive sets

These are the .zip archives made by `compile/compile_package.bat`  
These are generated under [./dist/](./dist/)  

Each additionally contains License and Copyright notices by way of these files:  

- [./LICENSES/LICENSE.gpl3](./LICENSES/LICENSE.gpl3)  
- [./LICENSES/LICENSE.mit](./LICENSES/LICENSE.mit)  
- [./NOTICE.txt](./NOTICE.txt)  

__NOTE!__ The binaries are NOT statically linked and are therefore by default __NOT portable__!  
They depend on the host machine to supply any otherwise missing libraries!  
This includes the 'Standalone' package.  

#### Full set (`DDCL_full.zip`)  

Contains the following:  

1. `DDCL-Setup.msi`
2. `DDCL.exe`
3. `installer.exe`
4. `uninstaller.exe`
5. `conf.toml`

#### Manual set (`DDCL_manual.zip`)  

Contains the following:  

1. `DDCL.exe`
2. `installer.exe`
3. `uninstaller.exe`
4. `conf.toml`

#### Standalone set (`DDCL_standalone.zip`)  

Contains the following:  

1. `DDCL.exe`
2. `conf.toml`

### Standalone executable use

1. Get an installation package
2. Move the `DDCL.exe` binary to the desired location.
3. Run it once to generate the default configuration file (config.toml) in the same directory.
4. Edit the config file with what disks to check for, IP address to check connection to,  
   UNC (SMB shares, etc.) paths to verify, DNS ovverride, expected DNS suffix, and more.
5. Run it again to start logging!

### Full install

1. Get an installation package
2. Choose one of two paths - Manual managed install or MSI install.  

   For Manual install:  

   1. Move the executables to the desired location.  
   2. Run `DDCL.exe` once to generate the default configuration file `conf.toml` in the same directory.  
   3. Edit the config file with what disks to check for, IP address to check connection to,  
      UNC (SMB, etc.) paths to verify, DNS ovverride, expected DNS suffix, and more.
   4. Run `installer.exe` to register the app in the start menu, %PATH%, and registry.  

   For MSI install:  

   1. Run `DDCL-Setup.msi`  
   The MSI should copy the files into `C:\Program Files\DDCL` and run `installer.exe` for you.

### Running  

Depending on how you install DDCL, you have a few options for executing it.  
Refer to the beneath table for applicable methods of execution by installation method :)  

| Execution method | Standalone EXE  | MSI | Installer binary  |
|:-----------------|:---------------:|:---:|:-----------------:|
| From directory   |        X        |  X  |         X         |
| Command (`ddcl`) |                 |  X  |         X         |
| Start menu       |                 |  X  |         X         |

'From directory' here includes launching from File Explorer and from a terminal (`.\DDCL.exe`/`start DDCL.exe`)  

#### First run  

When DDCL registers a first-time run, it will print the help message before exiting after 30 seconds.  
This help message can be found at [Commandline options](#commandline-options-for-ddclexe)  

### Logging  

Logs are written to one of the following locations, in the same prioritization  
(Edit the code if you want it to be different, dont be cheap. Learn some C++):  

- {log_path}\DDCL-Logs\DDCL_log-\<timestamp>.csv
- %LOCALAPPDATA%\DDCL-Logs\DDCL_log-\<timestamp>.csv
- %OneDriveCommercial%\DDCL-Logs\DDCL_log-\<timestamp>.csv
- %OneDrive%\DDCL-Logs\DDCL_log-\<timestamp>.csv
- C:\Users\%USERNAME%\DDCL-Logs\DDCL_log-\<timestamp>.csv
- &EXE_DIR&\DDCL-Logs\DDCL_log-\<timestamp>.csv

'\<timestamp>' is the time of launching the application, formatted as `%d.%m.%Y-%H_%M_%S`.  
'{log_path}' is the path configured in `conf.toml`, if available.  
'&EXE_DIR&' is the resolved directory in which the `DDCL.exe` binary lies. This is written to the registry key `HKLM\SOFTWARE\DDCL` as `InstallPath`.  

The log contains only the CHANGES in status and the initial state, with a timestamp and any other relevant information.

#### Log format  

The log is in CSV format (comma delimited ","), with the following columns:  

- `timestamp`: The date and time of the log entry.  
- `kind`: The type of the log entry, self descriptive.  
- `value`: The value of the log entry, self descriptive.  
- `info`: Any additional information about the log entry.  

Example of the initial status write:  

```csv
timestamp,kind,value,info
03.09.2026-08:50:47,registered_detections,internet_connectivity;ethernet;dns_resolution;vpn_connection;drive_availability;unc_availability,initial_status
03.09.2026-08:50:47,internet_connectivity,online
03.09.2026-08:50:47,ethernet,GUID:{DEADBEEF-...};FriendlyName:'Ethernet 16';Description:'Realtek USB GbE Family Controller #8';DNSSuffix:example.domain;MAC:'DE:AD:BE:EF:DE:AD';PrimaryDHCPv4:1.1.1.1;PrimaryDNS:1.1.1.1;PrimaryGateway:N/A,
03.09.2026-08:50:47,wlan,no_connection,
03.09.2026-08:50:47,dns_resolution,1.1.1.1:sys_dns;True 1.1.1.1:ns1.somewhere.com;True www.wikipedia.org:sys_dns;False www.wikipedia.org:ns1.somewhere.com;True
03.09.2026-08:50:47,vpn_connection,not_connected,
03.09.2026-08:50:47,drive_availability,available,C:home
03.09.2026-08:50:47,unc_availability,available,\\path\going\somewhere:
```

### Commandline options for DDCL.exe  

- -h, --help  
   Print a help message, sleep for 20 seconds, then exit.
- -c, --config  
   Print the current configuration to the console and exit.   
  -t --times:\<count>  
      Run detection loop \<count> times. Defaults to 0.  
      If \<count> is not given correctly, assumes 1.  

The help message is roughly as follows:  

```plaintext
===== DDCL v{version number} =====

{call} [-h|--help] [-c|--config]

  -h --help
      Display this help message
  -c --config
      Display a configuration summary
  -t --times:<count>
      Run detection loop <count> times. Defaults to 0.
      If <count> is not given correctly, assumes 1.

DDCL is a tool for surveying network and storage status changes.
Checks are performed once every second and logged to a location given through a fallback chain.
 (See documentation at https://github.com/DVP-F/DDCL for detail)
At the moment, logs will be written to {location}
```

including some string emplacement.

The configuration summary looks something like this:

```plaintext
=== Disk Drive Connection Logger (DDCL) v{version number} - Startup Summary ===
Commandline arguments: --config
Enabled Detections:
  Internet Connectivity, Ethernet, WLAN, DNS Resolution, VPN Connection, Drive Availability, UNC Availability,
Log Path: C:\some\path\DDCL-Logs\
Virtual Terminal Processing: Requested, Status: ON

Network:
  Check Host: 1.1.1.1
  DNS Server: ns1.somewhere.com
  Expected Domain Suffix: domain.org
  Expected VPN Hostname: .?hummina\.hummina.?

Disks:
  Local Drives: C
  UNC paths:
    \\localhost\C
    \\somewhere\else

```
