# Requirements for installation software  

1. Check 'HKLM:\Software\DDCL' `DWORD` key 'InstallType'  
   1. If 0; Standalone installer  
   2. If 1; [MSI](#msi)  

​  

## MSI  

### Registry  

Registry keys are to be written in a 64-bit view.  

The MSI will write the following registry keys:  

- HKLM:\Software\DDCL
  - `REG_SZ` InstallPath
  - `DWORD` InstallType

Therefore the following keys remain to be written, with their appropriate values.  

- HKLM:\Software\DDCL
  - `DWORD` InstallStatus
    - `value := 0 if install_completed else 1`
- HKLM:\Software\Microsoft\Windows\CurrentVersion\Run
  - `REG_SZ` DDCL
    - `value := install_path`
    - Optional
