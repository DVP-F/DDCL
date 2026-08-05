# TODO

## Remaining to be done  

1. Human review and read-through of DDCL.cpp  
    -> See if it's fairly understandable (semantic logic)  
2. Update scripts  
   -> `compile.sh` - maybe `.clangd`  
3. Testing  
    - DDCL.exe  
        -> Full use test  
        -> Hardware tests - stability  
    - Pre-production use  
        -> Test viability with active consumer use  
        -> Usability, configurability, looking up the log files.  
        -> Registry key use  
        -> Commandline use  
4. Prep for use in-prod  
    -> Update documentation and make techincal docs  
5. Remake installer.py in C or Rust  
   -> Minimize size in deployment (currently 7MB)  
6. Make an uninstaller  
   - split installers  
     -> msi ( use native registry key write )  
     -> manual/standalone  
   - uninstall differs too then:  
     -> msi - use native uninstall, but still just an additional bin.    
     -> manual/standalone - just an extra bin in the install folder  
7. Labels for UNC paths and local mounted drives  

## Table and checklist  

| Action                   | Information                                 | Done? |
|:-------------------------|:--------------------------------------------|:-----:|
| Human review of DDCL.cpp | Semantic logic                              |Delayed|
| Update scripts           | Based on installer.py / compile_package.bat |       |
| Testing DDCL.exe         | Full use test                               |       |
| Hardware tests           | Stability                                   |       |
| Pre-production use       | Viability in active consumer use            |       |
| Pre-production use       | Usability, configurability, log files.      |       |
| Pre-production use       | Registry key use                            |       |
| Pre-production use       | Commandline use                             |       |
| Prep for use in-prod     | Update documentation                        |       |
| Prep for use in-prod     | Technical docs                              |       |
| Rewrite `installer.py`   | Rust, C, Zig?                               |       |
| Redo (un)install         | uninstall.cpp                               |       |
| Redo (un)install         | Update msi script                           |       |
| Redo (un)install         | Update packaging scripts                    |       |

ADD reg write from msi 'InstallType' and use to direct (un)install instead of args

```plaintext
       ,
       \`-._           __
        \\  `-..____,.'  `.
         :`.         /    \`.
         :  )       :      : \
          ;'        '   ;  |  :
          )..      .. .:.`.;  :
         /::...  .:::...   ` ;
         ; _ '    __        /:\
         `:o>   /\o_>      ;:. `.
        `-`.__ ;   __..--- /:.   \
        === \_/   ;=====_.':.     ;
         ,/'`--'...`--....        ;
              ;                    ;
            .'                      ;
          .'                        ;
        .'     ..     ,      .       ;
       :       ::..  /      ;::.     |
      /      `.;::.  |       ;:..    ;
     :         |:.   :       ;:.    ;
     :         ::     ;:..   |.    ;
      :       :;      :::....|     |
      /\     ,/ \      ;:::::;     ;
    .:. \:..|    :     ; '.--|     ;
   ::.  :''  `-.,,;     ;'   ;     ;
.-'. _.'\      / `;      \,__:      \
`---'    `----'   ;      /    \,.,,,/
                   `----`              fsc
```
