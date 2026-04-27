
@echo off
REM Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
REM License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package `LICENSE.txt`
REM Source code hosts:
REM - GitHub: https://github.com/DVP-F/DDCL 

REM This compiles the python scripts into executables using the Nuitka python package and compiles the C++ runner using cl.exe
REM Then packages them into an MSI installer using the WiX Toolset

REM !! IMPORTANT NOTE :: DO NOT MODIFY QUOTING, ONLY CONTENTS !!
REM Batch is __stupidly__ sensitive to quoting but id rather not use ps 

REM flag for archives failing
set archive_failure=0

REM Define the metadata
REM Versions have to be four numbers
set            version=0.1.0.0
set      version_comma=0,1,0,0
set       company_name=CHANGEME
set company_short_name=CHANGEME || REM Should match ^[a-zA-Z_]+$, used in legal id of the msi package.
set        runner_name=DDCL
set     installer_name=DDCL_Installer
REM Output names
set         "runner_bin_name=%runner_name%.exe"
set      "installer_bin_name=%installer_name%.exe"
set            "msi_bin_name=%runner_name%-Setup.msi"
set       "archive_name_full=%runner_name%_full.zip"
set     "archive_name_manual=%runner_name%_manual.zip"
set "archive_name_standalone=%runner_name%_standalone.zip"
REM Wix PDB, equivalent name to msi_bin_name - this file will be deleted!
set             "wixpdb_name=%runner_name%-Setup.wixpdb"
REM Dirs
set output_dir=..\dist
set  start_dir="%cd%"
REM Compiler paths
set CLANG_PATH="C:\Program Files\...\cl.exe"
set    VC_VARS=C:\Program Files\Microsoft Visual Studio\...\vcvarsall.bat
set    RC_PATH="C:\Program Files (x86)\Windows Kits\...\rc.exe"
set  LINK_PATH="C:\Program Files\Microsoft Visual Studio\...\link.exe"
set   WIX_PATH="C:\Program Files\WiX Toolset v7.0\bin\wix.exe"
REM GUIDs for MSI package
REM __,,..-----'''¨¨¨¨ FORCEFIELD DO NOT TOUCH ¨¨¨¨'''-----..,,__
set "MSIPKG_ID=DDCL_MSI_INSTALLER_ref_%company_short_name%" || REM Legal ID for the MSI
set GUID_STABLE_UPGRADE=eec3969e-9ee5-4635-916d-01c318c849e7
set GUID_STABLE_RUNNER=5276fcd8-b925-4fb5-a886-cadbcd3f6cfe
set GUID_STABLE_INSTALLER=217bf05d-2d4a-41fa-92a6-10e1198fefc9
set GUID_STABLE_CONFIG=52b45464-0d16-40ea-ba59-eb1212372bd7
set GUID_STABLE_NOTICE=07f52426-4afa-4a18-80e8-8c1a44a41d30
set GUID_STABLE_LICENSE_GPL=35a50009-d180-4cf8-a3a1-32580f37309c
set GUID_STABLE_LICENSE_MIT=4a5cb89b-2a11-477e-94a7-438ac326b5ca
REM ¨¨´'**-----,,,____ FORCEFIELD DO NOT TOUCH ____,,,-----**'`¨¨
REM Product verion GUID. Change every new version
set GUID_PRODUCT_CODE=6ff5a40b-a57b-4fb9-86ff-742b570bb6db

REM You can install VS, Python, and WiX from here:
REM https://visualstudio.microsoft.com/downloads/	// At least 2022 edition, with C++ workload
REM https://www.python.org/downloads/				// At least Python 3.10
REM https://github.com/wixtoolset/wix/releases		// At least WiX CLI v7.0

REM Ensure paths exist
if not exist %output_dir% (
	mkdir %output_dir%
)
(( cd /d %output_dir% || cd /d "%output_dir%" ) > nul && echo Output directory is set to "%output_dir%" ) || echo Failed to set output directory. Please check the path and permissions.

REM This compiles the python scripts into executables using the Nuitka python package
REM Ensure Nuitka and ZStd is installed
python -m pip install nuitka > nul && echo Nuitka is installed! || echo Failed to ensure Nuitka is installed, aborting! && exit
python -m pip install zstandard > nul && echo ZStandard is installed! || echo Failed to ensure ZStandard is installed!
python -m pip install pyuac pywin32 > nul && echo PyUAC and PyWin32 are installed! || echo Failed to ensure PyUAC and PyWin32 are installed, aborting! && exit
REM Compile installer.py
python -m nuitka --mode=onefile --remove-output --include-windows-runtime-dlls=yes --windows-uac-admin --company-name="%company_name%" --product-name="%installer_name%" --file-version="%version%" --product-version="%version%" --output-dir="." --output-filename="%installer_bin_name%" --deployment ..\source\installer.py 

REM Generate the runner's resource file (runner.rc)
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

REM Set the environment vars for clang
call "%VC_VARS%" x64

REM Compile runner (DiskDriveConnectionLogger.cpp)
REM This assumes you have VS installed and it supports compiling C++17
%CLANG_PATH% /EHsc /std:c++17 /c ..\source\DDCL.cpp /Fo:"runner.obj"
if exist "runner.obj" (
	REM Compile the resource file - only run if cl compiled properly
	%RC_PATH% /fo "runner.res" "runner.rc"
)
if exist "runner.res" (
	REM Link the runner, resource, and libs - only run if cl and rc compiled properly
	%LINK_PATH% /SUBSYSTEM:CONSOLE "runner.obj" "runner.res" /out:"%runner_bin_name%"
)
REM This compilation process is needed to include metadata 

REM Delete intermediate files without throwing
if exist runner.obj (
	del runner.obj
)
if exist runner.rc (
	del runner.rc
)
if exist runner.res (
	del runner.res
)
if exist runner.exe (
	del runner.exe
)

REM Package into an MSI installer
REM This part is complex and requires the WiX Toolset or equivalent

REM Copy the config file to the output dir
copy ..\source\conf.toml conf.toml >nul && echo Config file copied to output directory! || echo Failed to copy config file to output directory!

REM Make sure to accep the WiX EULA: $ wix eula accept wix7

REM Write the WiX source file
(
	echo ^<?xml version="1.0" encoding="UTF-8"?^>
	echo ^<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui"^>
	echo   ^<Package Id="%MSIPKG_ID%" Name="%runner_name%" Language="1033" Version="%version%" Manufacturer="%company_name%" UpgradeCode="%GUID_STABLE_UPGRADE%" InstallerVersion="500" Compressed="yes" Scope="perMachine"^>
	echo     ^<MajorUpgrade Schedule="afterInstallInitialize" AllowSameVersionUpgrades="yes" AllowDowngrades="no" DowngradeErrorMessage="A newer version of %runner_name% is already installed." /^>
	echo     ^<MediaTemplate EmbedCab="yes" /^>
    echo     ^<StandardDirectory Id="ProgramFiles6432Folder"^>
    echo       ^<Directory Id="INSTALLFOLDER" Name="%runner_name%" /^>
    echo     ^</StandardDirectory^>
    echo     ^<DirectoryRef Id="INSTALLFOLDER"^>
    echo       ^<Directory Id="LicensesDir" Name="LICENSES" /^>
    echo     ^</DirectoryRef^>
    echo       ^<Component Id="RunnerComponent" Guid="%GUID_STABLE_RUNNER%"^>
    echo         ^<File Id="Runner" Name="%runner_bin_name%" Source="%runner_bin_name%" KeyPath="yes" /^>
    echo       ^</Component^>
    echo       ^<Component Id="InstallerComponent" Guid="%GUID_STABLE_INSTALLER%"^>
    echo         ^<File Id="Installer" Name="%installer_bin_name%" Source="%installer_bin_name%" KeyPath="yes" /^>
    echo       ^</Component^>
    echo       ^<Component Id="TomlComponent" Guid="%GUID_STABLE_CONFIG%"^>
    echo         ^<File Id="Config" Name="conf.toml" Source="../source/conf.toml" KeyPath="yes" /^>
    echo       ^</Component^>
    echo       ^<Component Id="NoticeComponent" Guid="%GUID_STABLE_NOTICE%"^>
    echo         ^<File Id="Notice" Name="NOTICE.txt" Source="../NOTICE.txt" KeyPath="yes" /^>
    echo       ^</Component^>
    echo        ^<ComponentGroup Id="LicenseComponents"^>
    echo            ^<Component Id="GPLLicenseComponent" Guid="%GUID_STABLE_LICENSE_GPL%" Directory="LicensesDir"^>
    echo                ^<File Id="GPLLicense" Name="LICENSE.gpl3" Source="../LICENSES/LICENSE.gpl3" KeyPath="yes" /^>
    echo            ^</Component^>
    echo            ^<Component Id="MITLicenseComponent" Guid="%GUID_STABLE_LICENSE_MIT%" Directory="LicensesDir"^>
    echo                ^<File Id="MITLicense" Name="LICENSE.mit" Source="../LICENSES/LICENSE.mit" KeyPath="yes" /^>
    echo            ^</Component^>
    echo        ^</ComponentGroup^>
	echo     ^<Feature Id="MainFeature" Title="MainFeature" Level="1"^>
	echo       ^<ComponentRef Id="RunnerComponent" /^>
	echo       ^<ComponentRef Id="InstallerComponent" /^>
	echo       ^<ComponentRef Id="TomlComponent" /^>
	echo       ^<ComponentRef Id="NoticeComponent" /^>
	echo       ^<ComponentGroupRef Id="LicenseComponents"/^>
	echo     ^</Feature^>
	echo     ^<CustomAction Id="SetLaunchApp1" Property="LaunchApp1" Value="&quot;[#Installer]&quot;" /^>
	echo     ^<CustomAction Id="LaunchApp1" ExeCommand="[LaunchApp1]" Execute="deferred" Directory="INSTALLFOLDER" Impersonate="no" Return="asyncNoWait" /^>
	echo     ^<InstallExecuteSequence^>
	echo       ^<Custom Action="SetLaunchApp1" After="CostFinalize"/^>
	echo       ^<Custom Action="LaunchApp1" Before="InstallFinalize"/^>
	echo     ^</InstallExecuteSequence^>
	echo   ^</Package^>
	echo ^</Wix^>
) > installer.wxs

echo Compiling MSI...
%WIX_PATH% build installer.wxs -o %msi_bin_name% -ext WixToolset.Util.wixext  && echo Success! || echo Failed to compile MSI package!

REM Clean up intermediate files
if exist installer.wxs (
	del installer.wxs
)
if exist "%wixpdb_name%" (
	del "%wixpdb_name%"
)

REM Create temp LICENSES dir
mkdir LICENSES
copy ..\LICENSES\* LICENSES\ 1>nul && echo Licenses copied! || echo Failed to copy licenses!
REM Make ZIP archives
echo Compressing...
REM These all also include the notice and licenses per GPL-3.0 requirements (§4)
REM Full package (MSI, runner, installer, conf.toml)
powershell -Command Compress-Archive -Path ..\source\conf.toml, "%runner_bin_name%", "%installer_bin_name%", "%msi_bin_name%", LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_full%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1
REM Manual setup (runner, installer.bat, conf.toml)
powershell -Command Compress-Archive -Path conf.toml, "%runner_bin_name%", ..\source\installer.bat, LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_manual%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1
REM Standalone (runner, conf.toml)
powershell -Command Compress-Archive -Path conf.toml, "%runner_bin_name%", LICENSES, ..\NOTICE.txt -DestinationPath "%archive_name_standalone%" -Force -CompressionLevel Optimal 1>nul && echo Success! || echo Failed compression! && set archive_failure=1
REM remove temp license dir
rmdir /s /q LICENSES	

REM Final output message
echo.
if exist "%msi_bin_name%" (
	echo Compilation and packaging complete. The installer is located at "%output_dir%\%msi_bin_name%".
) else (
	echo Compilation or packaging failed. Please check the output for errors.
)
if "%archive_failure%"=="1" (
	echo Compression failed.
) else (
	echo Compression complete. Archives are located in "%output_dir%\".
)
cd "%start_dir%"
