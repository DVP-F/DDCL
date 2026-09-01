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
5. Labels for UNC paths and local mounted drives  
6. Update docs before version bump to 0.2.1

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
| Redo (un)install         | Update packaging scripts                    |  0.5  |

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
