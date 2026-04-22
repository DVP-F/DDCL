@echo off
REM Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
REM License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package under `LICENSES`
REM Source code hosts:
REM - GitHub: https://github.com/DVP-F/DDCL 
REM See `NOTICE.txt` for further Licensing information

REM Elevate to admin immediately

::::::::::::::::::::::::::::::::::::::::::::
:: Elevate.cmd - Version 8 
:: // Silenced version // 
:: Automatically check & get admin rights
:: see "https://stackoverflow.com/a/12264592/1016343" for description
::::::::::::::::::::::::::::::::::::::::::::
 CLS
:init
 setlocal DisableDelayedExpansion
 set cmdInvoke=1
 set winSysFolder=System32
 set "batchPath=%~dpnx0"
 rem this works also from cmd shell, other than %~0
 for %%k in (%0) do set batchName=%%~nk
 set "vbsGetPrivileges=%temp%\OEgetPriv_%batchName%.vbs"
 setlocal EnableDelayedExpansion
:checkPrivileges
  whoami /groups /nh | find "S-1-16-12288" > nul
  if '%errorlevel%' == '0' ( goto checkPrivileges2 ) else ( goto getPrivileges )
:checkPrivileges2
  net session 1>nul 2>NUL
  if '%errorlevel%' == '0' ( goto gotPrivileges ) else ( goto getPrivileges )
:getPrivileges
  if '%1'=='ELEV' (echo ELEV & shift /1 & goto gotPrivileges)
  ECHO Set UAC = CreateObject^("Shell.Application"^) > "%vbsGetPrivileges%"
  ECHO args = "ELEV " >> "%vbsGetPrivileges%"
  ECHO For Each strArg in WScript.Arguments >> "%vbsGetPrivileges%"
  ECHO args = args ^& strArg ^& " "  >> "%vbsGetPrivileges%"
  ECHO Next >> "%vbsGetPrivileges%"
  if '%cmdInvoke%'=='1' goto InvokeCmd 
  ECHO UAC.ShellExecute "!batchPath!", args, "", "runas", 1 >> "%vbsGetPrivileges%"
  goto ExecElevation
:InvokeCmd
  ECHO args = "/c """ + "!batchPath!" + """ " + args >> "%vbsGetPrivileges%"
  ECHO UAC.ShellExecute "%SystemRoot%\%winSysFolder%\cmd.exe", args, "", "runas", 1 >> "%vbsGetPrivileges%"
:ExecElevation
 "%SystemRoot%\%winSysFolder%\WScript.exe" "%vbsGetPrivileges%" %*
 exit /B
:gotPrivileges
 setlocal & cd /d %~dp0
 if '%1'=='ELEV' (del "%vbsGetPrivileges%" 1>nul 2>nul  &  shift /1)
 ::::::::::::::::::::::::::::
 :: START
 ::::::::::::::::::::::::::::

setlocal

set "INSTALL_PATH=%cd%"

REM Check files in current directory
if not exist "DDCL.exe" (
    echo DDCL.exe not found in current directory: %cd%
    call :flag_failed
    exit /b 1
)
if not exist "conf.toml" (
    echo conf.toml not found in current directory: %cd%
    call :flag_failed
    exit /b 1
)

REM Init registry keys
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /ve /t REG_SZ /d "" /f >nul 2>&1
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /v InstallPath /t REG_SZ /d "%INSTALL_PATH%" /f >nul 2>&1
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /v InstallStatus /t DWORD /d 0 /f >nul 2>&1

REM Create Start Menu folder and shortcut
mkdir "C:\ProgramData\Microsoft\Windows\Start Menu\Programs\DDCL" 2>nul

REM Create shortcut via VBS ( yuckily )
(
echo Set WshShell = CreateObject^("WScript.Shell"^)
echo Set oLink = WshShell.CreateShortcut^("C:\ProgramData\Microsoft\Windows\Start Menu\Programs\DDCL\DDCL.lnk"^)
echo oLink.TargetPath = "%INSTALL_PATH%\DDCL.exe"
echo oLink.WorkingDirectory = "%INSTALL_PATH%"
echo oLink.Save
) > "%temp%\create_lnk.vbs"
cscript //nologo "%temp%\create_lnk.vbs" >nul 2>&1 && del "%temp%\create_lnk.vbs" 2>nul

REM Add to PATH (system-wide)
setx /M PATH "%PATH%;%INSTALL_PATH%" >nul 2>&1
if errorlevel 1 (
    echo Warning: Failed to update PATH ^(new sessions only^)
    call :flag_failed
)

echo.
echo Installation completed successfully!
echo - Registry keys created under HKLM\SOFTWARE\DDCL
echo - Start Menu shortcut added
echo - PATH updated ^(restart required for new sessions^)
echo.
exit /b 0

:flag_failed
echo.
echo Installation failed - registry marked as failed ^(InstallStatus=1^).
timeout /t 5 >nul
reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /v InstallStatus /t QWORD /d 1 /f >nul 2>&1
exit /b 1