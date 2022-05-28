# Cleanup
 [![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE.md) [![GitHub all releases](https://img.shields.io/github/downloads/cregx/mdt-cleanup-harddrive-winpe/total?color=green&label=download%20all%20releases)](https://github.com/cregx/mdt-cleanup-harddrive-winpe/releases)

The WinPE UI application "cleanup" provides the option of a simple wipe of the primary disk before a Microsoft Deployment Toolkit (MDT) based Lite Touch installation begins.

Cleanup is a Win32 application created in Microsoft C and Visual Studio 2010.

## My problem origin
I noticed in an MDT project to deploy an offline based Windows 10 installation that it sometimes interrupted with an error.
This happened somewhere in the task sequence after the step: "Format and Partition Disk (BIOS) / ... (UEF)", i.e. relatively early, at the beginning of the installation process. 

I was able to trace this back to a hard disk that had not been cleaned (I also found some posts on the Internet that pointed to the problem). 

The solution to the problem was also relatively simple: I had to do nothing other than run the Diskpart tool in a CMD shell before starting the actual  installation:

```
diskpart.exe
sel dis 0
clean
exit
```

### An alternative to a pure script-based solution
In the long run, however, I did not like the manual solution and so an additional step in the task sequence was supposed to solve the problem for me.
However, it quickly turns out that this was not so easy in the MDT because I needed to reboot the system after cleaning up the hard disk and so the task sequence would run over and over again (there may be a solution for this too - but I don't know it).

I found out that you can run a simple Visual Basic script with this task before running the actual installation. That was the solution!

However, since I wanted to make the script a little more pleasant (nicer), I had to come up with a GUI-based solution: cleanup.
<p align="center" width="100%">
<img alt="Cleanup UI" src="https://user-images.githubusercontent.com/14788832/170817591-201cce66-7ee0-417f-8f83-7bde98e36e92.png" width="40%" height="40%" />
</p>

#### Components of the solution
The solution consists of the following three components:

- cleanup.exe: the GUI-based application
- action.bat: batch script, which is responsible for running Diskpart
- diskpart.txt: parameter file with instructions for Diskpart

#### Rough flowchart
<p align="center" width="100%">
<img alt="Cleanup rough flowchar" src="https://user-images.githubusercontent.com/14788832/170816759-b73ac160-d741-463c-8e67-82d7d79252ab.svg" width="35%" height="35%" />
</p>

### Implement cleanup in own LTI-Deployment
Todo: must still be written

## Code of Conduct

Please refer to the [Code of Conduct](https://github.com/cregx/mdt-cleanup-harddrive-winpe/blob/main/CODE_OF_CONDUCT.md) for this repository.

## Disclaimer

This program code is provided "as is", without warranty or guarantee as to its usability or effects on systems. It may be used, distributed and modified in any manner, provided that the parties agree and acknowledge that the author(s) assume(s) no responsibility or liability for the results obtained by the use of this code.
