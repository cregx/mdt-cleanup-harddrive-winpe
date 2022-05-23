@echo off
REM
REM Common description:
REM  This script is used to clean up the (primary) hard disk using Diskpart.
REM  Version 1.0 (Revision 0)
REM  Author: Christoph Regner (Mai 2022)
REM
REM Parameter description:
REM  Used parameter: %1
REM  File name and path to the diskpart.txt script, e.g. diskpart.txt.
REM  This script contains all necessary instructions for diskpart.
REM  More info at: https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/diskpart-scripts-and-examples
REM 
REM Return values:
REM  The script must return the value 0x400 (1024) in case of successful processing (E_SUCCESS).
REM  In case of an failure the script should return a different value (!= 0x400).
REM  This will then be displayed as an error in the GUI after processing is complete.
REM
REM  Note: The return value of diskpart of 0x0 (0) represents a success, but is overwritten with E_SUCCESS.
REM  The return value 0xFFFFFFFFFFFFFFFF (-1) as well as 0xC000013A (3221225786)
REM  are reserved for internal purposes and therefore must not be used. For more information,
REM  see the project files for the "cleanup" project.

SET /A E_SUCCESS=0x400
SET /A errno^|=%E_SUCCESS%

REM BEGIN DEBUG Test
REM SET /A errno^|=0x10
REM END DEBUG Test
 
echo Operation: diskpart /s %1
echo Start clean up process.
diskpart.exe /s %1 

if %errorlevel% neq 0 (
	EXIT /B %errorlevel%
) 
EXIT /B %errno%