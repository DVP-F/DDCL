# TODO

## Remaining to be done  

1. Human review and read-through of DDCL.cpp  
    -> See if it's fairly understandable (semantic logic)  
2. Update scripts  
   -> `compile.sh` - maybe `.clangd`  
3. Testing  
    - DDCL.exe  
        -> Full use test, endurance, commandline arguments  
        -> Hardware tests - timing, stability, ram and cpu use  
    - Pre-production use  
        -> Test viability with active consumer use  
        -> Usability, configurability, looking up the log files.  
        -> Registry key use  
        -> Commandline use  
4. Prep for use in-prod  
    -> Update documentation and make techincal docs  
5. Remake installer.py in C or Rust  
   -> Minimize size in deployment (currently 7MB)  

## Table and checklist  

| Action                   | Information                                 | Done? |
|:-------------------------|:--------------------------------------------|:-----:|
| LLM review               | Implemented critical fixes                  |   X   |
| Cleanup                  | Remove unused / unneeded function           |   X   |
| Cleanup                  | Use `detection_kinds`                       |   X   |
| Reorganize repo          | Update paths                                |   X   |
| Human review of DDCL.cpp | Semantic logic                              |Delayed|
| Update scripts           | Based on installer.py / compile_package.bat |       |
| Testing DDCL.exe         | Full use test                               |       |
| Testing DDCL.exe         | Endurance                                   |       |
| Testing DDCL.exe         | Commandline arguments                       |       |
| Hardware tests           | Timing of updates                           |       |
| Hardware tests           | Stability                                   |       |
| Hardware tests           | RAM and CPU use                             |       |
| Installers               | Write to correct location (MSI)             |   X   |
| Installers               | Test registry key writes and %PATH% update  |   X   |
| Installers               | Check .lnk write                            |   X   |
| Pre-production use       | Viability in active consumer use            |       |
| Pre-production use       | Usability, configurability, log files.      |       |
| Pre-production use       | Registry key use                            |       |
| Pre-production use       | Commandline use                             |       |
| Prep for use in-prod     | Clear out files and old remains             |   X   |
| Prep for use in-prod     | Update documentation                        |       |
| Prep for use in-prod     | Technical docs                              |       |
| Prep for use in-prod     | Reset git log                               |   X   |
| Prep for use in-prod     | Publish repository                          |   X   |
| Prep for use in-prod     | Package archives and binaries               |   X   |
| Prep for use in-prod     | Release and distribute                      |   X   |
| Rewrite `installer.py`   | Rust, C, Zig?                               |       |
