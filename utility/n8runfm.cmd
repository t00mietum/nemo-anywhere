@echo off
SETLOCAL

::	Purpose:
::		Lets 'n8runfm' be run from cmd.exe, Win+R, the Start menu and Task
::		Scheduler, none of which can execute a .ps1 directly: having .PS1 in
::		PATHEXT only makes cmd hand the file to ShellExecute, and the default
::		.ps1 association opens an editor rather than running it.
::		PowerShell itself does not need this shim - it resolves the bare name
::		to n8runfm.ps1 in PATH on its own, and prefers the .ps1 over this .cmd,
::		so this file never shadows normal pwsh usage.
::		Sits next to n8runfm.ps1 and finds it via %~dp0, so the pair can be
::		moved or re-synced together without editing anything.
::	History:
::		- 20260802 JC: Created.

::----------------------------------------------------------------------------
:MAIN

	set PSFILE=%~dp0n8runfm.ps1

	::
	:: Validate
	::

	:: Script
	if exist "%PSFILE%" goto :OK005
		echo Not found: "%PSFILE%"
		goto :ERROR
	:OK005

	:: PowerShell 7. The script is pwsh-only, so do not fall back to the
	:: Windows PowerShell 5.1 that ships in the box.
	where /q pwsh.exe
	if not errorlevel 1 goto :OK010
		echo PowerShell 7 ^(pwsh.exe^) was not found on PATH.
		goto :ERROR
	:OK010

	::
	:: Execute
	::
	pwsh.exe -NoProfile -File "%PSFILE%" %*
	set RC=%ERRORLEVEL%

ENDLOCAL & exit /b %RC%

::----------------------------------------------------------------------------
:ERROR
	echo [ An error occurred. ]
ENDLOCAL & exit /b 1
