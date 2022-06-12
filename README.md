# Cleanup
 [![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE.md) [![GitHub all releases](https://img.shields.io/github/downloads/cregx/mdt-cleanup-harddrive-winpe/total?color=green&label=download%20all%20releases)](https://github.com/cregx/mdt-cleanup-harddrive-winpe/releases)

The WinPE UI application "cleanup" provides the ability to easily wipe the primary hard disk before a Microsoft Deployment Toolkit (MDT) based Lite Touch installation begins, i.e. before the LTI Wizard starts.

Cleanup is a Win32 application created in (Microsoft) C and Visual Studio 2010. Of course, you can also compile the project using a newer version of Visual Studio, such as 2019, or you can use an already compiled release version.

## The origin of my problem
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

However, since I wanted to make the script a little more pleasant (nicer :blush:), I had to come up with a GUI-based solution: **cleanup**.
<p align="center" width="100%">
<img alt="Cleanup UI" src="https://user-images.githubusercontent.com/14788832/170817591-201cce66-7ee0-417f-8f83-7bde98e36e92.png" width="50%" height="50%" />
</p>

#### Components of the solution
The solution consists of the following three components:

- cleanup.exe: the GUI-based application
- action.bat: batch script, which is responsible for running Diskpart
- diskpart.txt: parameter file with instructions for Diskpart

#### Rough flowchart
<p align="center" width="100%">
<img alt="Cleanup rough flowchar" src="https://user-images.githubusercontent.com/14788832/170816759-b73ac160-d741-463c-8e67-82d7d79252ab.svg" width="45%" height="45%" />
</p>

### Implement cleanup in own LTI-Deployment (step by step instructions)
1. [Download](https://github.com/cregx/mdt-cleanup-harddrive-winpe/releases) a current release version of Cleanup (or compile your own customised version).
2. Place the three files (cleanup.exe, action.bat, diskpart.txt) in a new directory - for example, in an offline deployment share, e.g. under __C:\DeploymentShare Extra\Cleanup__.
3. Modify the following template __C:\Program Files\Microsoft Deployment Toolkit\Templates\Unattend_PE_x64.xml__ as shown below. This will cause Cleanup to run immediately before Lite Touch PE starts and thus the Installation Wizard. Look for the first occurrence of the ```<RunSynchronousCommand>``` tag in the text and duplicate it. Do not forget to set the _Order tag_ for the Lite Touch PE to 2 if you have duplicated the code region.

```{.xml .numberLines .lineAnchors}
<?xml version="1.0" encoding="utf-8"?>
<unattend xmlns="urn:schemas-microsoft-com:unattend">
    <settings pass="windowsPE">
        <component name="Microsoft-Windows-Setup" processorArchitecture="amd64" publicKeyToken="21cf1116nd936e98" language="neutral" versionScope="nonSxS" xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State">
            <Display>
                <ColorDepth>32</ColorDepth>
                <HorizontalResolution>1024</HorizontalResolution>
                <RefreshRate>60</RefreshRate>
                <VerticalResolution>768</VerticalResolution>
            </Display>
            <RunSynchronous>
                <!-- BEGIN CLEANUP -->
                <RunSynchronousCommand wcm:action="add">
                    <Description>Prepare hard drive (Cleanup)</Description>
                    <Order>1</Order>
                    <Path>x:\cleanup.exe</Path>
                </RunSynchronousCommand>
                <!-- END CLEANUP -->
                <RunSynchronousCommand wcm:action="add">
                    <Description>Lite Touch PE</Description>
                    <Order>2</Order>
                    <Path>wscript.exe X:\Deploy\Scripts\LiteTouch.wsf</Path>
                </RunSynchronousCommand>
            </RunSynchronous>
        </component>
    </settings>
</unattend>
```
4. Change the properties of DeploymentShare as shown below. Alternative: If you are using an offline media instead, do it there (MDT Deployment Share \ Advanced Configuration \ Media \ (e.g. MEDIA001).
<p align="center" width="100%">
<img alt="MDT DeploymentShare Properties WinPE" src="https://user-images.githubusercontent.com/14788832/170854434-abb896b6-2593-4192-912b-b2a8e3993811.png" width="50%" height="50%" />
</p>

5. Create a new medium and boot from it.

6. The result in action: After integrating Cleanup into your own MDT solution, the result should look like the animation shown below. Have fun and success with it. :wink:

<p align="center" width="100%">
<img alt="MDT Cleanup Harddrive Animation" src="https://user-images.githubusercontent.com/14788832/172863867-bee55c1a-6be9-49ec-a8ce-16b7222bfcc5.gif" width="75%" height="75%" />
</p>

## FAQ

### :question: How much time should I expect to spend trying out the solution in my environment?

It is very difficult to give a general answer. But if you can already create ISO images with your installation, then this should not take more than **5-10 minutes**.

### :question: How to get cleanup in my local language (National Language Support)?

You have to change the resource file cleanup.rc and recompile the project.
You can find the corresponding resource entries for your language in the ```String Table```. These all contain the suffix ```_NLS``` in their identifier, e.g. ```IDS_RUN_ACTION_FAILED_NLS```. 
Just revise all the strings you want and then create a new build. Finally, you can find your language version under ```x64/Release_NLS```.

### :question: I have included Cleanup in my LTI solution, but I would like to remove it now. How can I do that?

You need to undo the following adjustments:

- the entry in ```C:\Program Files\Microsoft Deployment Toolkit\Templates\Unattend_PE_x64.xml```,
- of the property in DeploymentShare (```Extra directory to add```). After that the directory with the release files (e.g. ```C:\DeploymentShare Extra\Cleanup```) can be deleted.
- Finally, and most importantly, **delete** the ```content``` directory in the ```DeploymentMedia``` folder and **update** the ```Deployment Share``` (this will completely recreate this folder). If you omit this step, the files cleanup.exe, diskpart.txt and action.bat will remain in your WinPE (ISO file).

## Code of Conduct

Please refer to the [Code of Conduct](https://github.com/cregx/mdt-cleanup-harddrive-winpe/blob/main/CODE_OF_CONDUCT.md) for this repository.

## Disclaimer

This program code is provided "as is", without warranty or guarantee as to its usability or effects on systems. It may be used, distributed and modified in any manner, provided that the parties agree and acknowledge that the author(s) assume(s) no responsibility or liability for the results obtained by the use of this code.
