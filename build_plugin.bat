@echo off
set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set VCXPROJ="D:\Users\Patrick\OneDrive\Documents\Development\C\TeamSpeak3WebsitePreview\ts3websitepreview\ts3websitepreview.vcxproj"

echo === Building Release Win32 ===
%MSBUILD% %VCXPROJ% /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /nologo /verbosity:minimal

echo.
echo === Building Release x64 ===
%MSBUILD% %VCXPROJ% /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /nologo /verbosity:minimal
