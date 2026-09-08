@echo off
SETLOCAL

::	Purpose:
::		Lets 'runfm' be run from cmd.exe, Win+R, the Start menu and a shortcut,
::		none of which can execute a .ps1 directly: having .PS1 in PATHEXT only
::		makes cmd hand the file to ShellExecute, and the default .ps1
::		association opens an editor rather than running it.
::		The launcher self-elevates, so nothing here needs to.
::	History:
::		- 20260908 JC: Second spelling of the synced tree as a fallback.
::		- 20260907 JC: Created.

::----------------------------------------------------------------------------
:MAIN

	:: Four levels up from mswin\cli\by-self\cmd\ is the util dir, next to which
	:: 0_crossplatform sits. Fall back to the home-relative spelling for a copy
	:: dropped somewhere else.
	set PSFILE=%~dp0..\..\..\..\0_crossplatform\n8runfm.ps1
	if exist "%PSFILE%" goto :OK005
	set PSFILE=%USERPROFILE%\synced\0-0\common\exec\util\0_crossplatform\n8runfm.ps1
	if exist "%PSFILE%" goto :OK005
	set PSFILE=%USERPROFILE%\Dropbox\0-0\common\exec\util\0_crossplatform\n8runfm.ps1
	if exist "%PSFILE%" goto :OK005
		echo Not found: n8runfm.ps1
		goto :ERROR
	:OK005

	:: PowerShell 7. The launcher is pwsh-only, so do not fall back to the
	:: Windows PowerShell 5.1 that ships in the box.
	where /q pwsh.exe
	if not errorlevel 1 goto :OK010
		echo PowerShell 7 ^(pwsh.exe^) was not found on PATH.
		goto :ERROR
	:OK010

	set N8RUNFM_WRAPPER=%~f0
	pwsh.exe -NoProfile -ExecutionPolicy Bypass -File "%PSFILE%" %*
	set RC=%ERRORLEVEL%

ENDLOCAL & exit /b %RC%

::----------------------------------------------------------------------------
:ERROR
	echo [ An error occurred. ]
ENDLOCAL & exit /b 1