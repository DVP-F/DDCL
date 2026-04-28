# [compile_package.bat](../compile/compile_package.bat)  

Compiling and packaging script.  
By default uses Nuitka, CLang, RC, WiX, CLang Linker.  
Wrapped in `setlocal`/`endlocal`.  

Generates the following under [../dist/](../dist/):  

- conf.toml
- DDCL.exe
- DDCL_Installer.exe
- DDCL-Setup.msi
- DDCL_full.zip
- DDCL_manual.zip
- DDCL_standalone.zip

In addition to these temporary and later removed files:

- installer.build/
- installer.dist/
- runner.rc
- runner.obj
- runner.res
- installer.wxs
- DDCL-Setup.wixpdb
- LICENSES/
  - LICENSE.gpl3
  - LICENSE.mit

The runner `DDCL.exe` is compiled using CLang and a resource file with the metadata.

```bat
(
    echo VS_VERSION_INFO VERSIONINFO
    echo FILEVERSION %version_comma%
    echo PRODUCTVERSION %version_comma%
    echo BEGIN
    echo   BLOCK "StringFileInfo"
    echo   BEGIN
    echo     BLOCK "040904B0"
    echo     BEGIN
    echo       VALUE "ProductName", "%runner_name%"
    echo       VALUE "FileVersion", "%version_comma%"
    echo       VALUE "ProductVersion", "%version_comma%"
    echo     END
    echo   END
    echo END
) >  "runner.rc"

call "%VC_VARS%" x64

%CLANG_PATH% /EHsc /std:c++17 /c ..\source\DDCL.cpp /Fo:"runner.obj"
if exist "runner.obj" (
    %RC_PATH% /fo "runner.res" "runner.rc"
)
if exist "runner.res" (
    %LINK_PATH% /SUBSYSTEM:CONSOLE "runner.obj" "runner.res" /out:"%runner_bin_name%"
)
```

The VC_VARS call is to initiate the environment for compilation.  
The stack of ifs guarantees that if one stage of compilation fails, the rest won't execute.  

After this, the following files are copied in:

- ../source/
  - conf.toml
- ../LICENSES/
  - LICENSE.gpl3
  - LICENSE.mit

And the MSI is compiled using a WiX source file:

```bat
(
    echo ^<?xml version="1.0" encoding="UTF-8"?^>
    echo ^<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui"^>
    REM I'm not including the entire source file here
    echo ^</Wix^>
) > installer.wxs

echo Compiling MSI...
%WIX_PATH% build installer.wxs -o %msi_bin_name% -ext WixToolset.Util.wixext  && echo Success! || echo Failed to compile MSI package!

if exist installer.wxs (
    del installer.wxs
)
if exist "%wixpdb_name%" (
    del "%wixpdb_name%"
)
```

Finally, the gathered files are archived as ZIP files.  

```bat
powershell -Command Compress-Archive -Path ..\source\conf.toml, "%runner_bin_name%", "%installer_bin_name%", "%msi_bin_name%", LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_full%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1

powershell -Command Compress-Archive -Path conf.toml, "%runner_bin_name%", ..\source\installer.bat, LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_manual%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1

powershell -Command Compress-Archive -Path conf.toml, "%runner_bin_name%", LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_standalone%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1
```

And we're done!  
