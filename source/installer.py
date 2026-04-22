# Copyright (c) DVP-F/Carnx00 2026 <carnx@duck.com>
# License : GNU GPL 3.0 (https://www.gnu.org/licenses/gpl-3.0.html) supplied with the package under `LICENSES`
# Source code hosts:
# - GitHub: https://github.com/DVP-F/DDCL 
# See `NOTICE.txt` for further Licensing information

#? trimmed down imports to minimum
from os import getcwd, path
from subprocess import run as srun, check_output, DEVNULL
from sys import argv
from win32com.client import Dispatch as COMDispatch
from pyuac import main_requires_admin

install_path:str = ""

def _flag_failed_setup():
	# Overwrite the key
	srun(r'reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /v InstallStatus /reg:64 /t DWORD /d 1 /f', shell=True)

def _find_installation_path() -> str|None:
	try:
		# Check default install locations (MSI installs to PF 64 or 32)
		for temp_path in [ r"C:\Program Files\DDCL", r"C:\Program Files (x86)\DDCL"]:
			if path.exists(temp_path):
				return path.abspath(temp_path)
		# Check where the program is called from
		if "DDCL" in getcwd():
			return path.abspath(getcwd())
		# Check how the program is called
		call = argv[0]
		if "\\" in call: # (a path was used)
			call_path = path.abspath(call)
			if "DDCL" in call_path: return path.dirname(call_path)
		# Scan uninstall entries
		for l in check_output(
			r'reg query "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall" /f DDCL /k /s 2>nul', 
			shell=True, errors='ignore', stderr=DEVNULL).decode('latin1').splitlines():
			if 'InstallLocation' in l and 'REG_SZ' in l:
				temp_path = l.strip().split()[-1]
				if path.exists(temp_path) or path.isdir(temp_path):
					return path.abspath(temp_path)
		return None
	except: pass

def _init_registry_keys() -> None:
	srun(r'reg add HKEY_LOCAL_MACHINE\SOFTWARE\DDCL /ve /reg:64 /t REG_SZ /d "" /f', shell=True) # Empty key
	# Prep install path value
	srun(fr'reg add HKEY_LOCAL_MACHINE\SOFTWARE\DDCL /v InstallPath /reg:64 /t REG_SZ /d "{
		install_path if install_path else ''}" /f', shell=True)
	# Add flag for install status
	srun(r'reg add "HKEY_LOCAL_MACHINE\SOFTWARE\DDCL" /v InstallStatus /reg:64 /t REG_DWORD /d 0 /f', shell=True)

def _add_lnk()->bool:
	try:
		srun(
			r'cd /d "C:\ProgramData\Microsoft\Windows\Start Menu\Programs" >nul && mkdir DDCL', 
			shell=True, errors='ignore', stderr=DEVNULL)
		# Yucky winapi COM object
		shortcut = (COMDispatch("WScript.Shell")
					.CreateShortCut(r"C:\ProgramData\Microsoft\Windows\Start Menu\Programs\DDCL\DDCL.lnk"))
		shortcut.Targetpath = path.join(install_path, "DDCL.exe")
		shortcut.WorkingDirectory = install_path
		shortcut.Save()
		return path.exists(path.join(install_path, "DDCL.exe"))
	except:
		return False

def _add_to_path()->bool:
	try:
		srun(rf'setx /M PATH "%PATH%;{install_path}"', shell=True)
		return True
	except: return False

# Automatically run as admin on windows
@main_requires_admin(return_output=False, scan_for_error=False)
def _install() -> None:
	global install_path
	try:
		install_path = _find_installation_path()
		if not install_path:
			raise LookupError(
				"Could not find installation path. Please run this installer from the DDCL installation directory.")
		_init_registry_keys()
		if not _add_lnk(): raise FileNotFoundError("_add_lnk() call failed to create link object.")
		if not _add_to_path(): raise RuntimeError("Failed to add to %PATH%!")
	except Exception as e:
		print(e)
		# Flag failure if any calls fail
		_flag_failed_setup()

# Call _install() on run
if __name__ == "__main__": _install() 
