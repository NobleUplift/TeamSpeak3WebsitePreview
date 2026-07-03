@echo off
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set ROOT=D:\Users\Patrick\OneDrive\Documents\Development\C\TeamSpeak3WebsitePreview
set LIBICONV_VCXPROJ="%ROOT%\libiconv\libiconv.vcxproj"
set VCXPROJ="%ROOT%\ts3websitepreview\ts3websitepreview.vcxproj"
set RELEASE=%ROOT%\Release
set MSFLAGS=/p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0 /nologo /verbosity:minimal

echo === Cleaning staging ===
if exist "%RELEASE%\staging_win32" rmdir /s /q "%RELEASE%\staging_win32"
if exist "%RELEASE%\staging_x64"  rmdir /s /q "%RELEASE%\staging_x64"
if not exist "%RELEASE%" mkdir "%RELEASE%"

echo.
echo === Building libiconv Win32 ===
%MSBUILD% %LIBICONV_VCXPROJ% /p:Configuration=Release /p:Platform=Win32 %MSFLAGS%
if errorlevel 1 goto :fail

echo.
echo === Building libiconv x64 ===
%MSBUILD% %LIBICONV_VCXPROJ% /p:Configuration=Release /p:Platform=x64 %MSFLAGS%
if errorlevel 1 goto :fail

echo.
echo === Building plugin Win32 ===
%MSBUILD% %VCXPROJ% /p:Configuration=Release /p:Platform=Win32 %MSFLAGS%
if errorlevel 1 goto :fail

echo.
echo === Building plugin x64 ===
%MSBUILD% %VCXPROJ% /p:Configuration=Release /p:Platform=x64 %MSFLAGS%
if errorlevel 1 goto :fail

echo.
echo === Packaging Win32 ===
copy /Y "%ROOT%\ts3websitepreview\lib\libcurl.dll" "%RELEASE%\staging_win32\plugins\ts3websitepreview\"
if errorlevel 1 goto :fail
copy /Y "%ROOT%\ts3websitepreview\lib\libxml2.dll" "%RELEASE%\staging_win32\plugins\ts3websitepreview\"
if errorlevel 1 goto :fail
powershell -NoProfile -Command "(Get-Content '%ROOT%\ts3websitepreview\package.ini') -replace '\{PLATFORM\}', 'win32' | Set-Content -Encoding ASCII '%RELEASE%\staging_win32\package.ini'"
if errorlevel 1 goto :fail
if exist "%RELEASE%\ts3websitepreview_win32.ts3_plugin" del "%RELEASE%\ts3websitepreview_win32.ts3_plugin"
powershell -NoProfile -Command "Add-Type -Assembly 'System.IO.Compression.FileSystem'; [System.IO.Compression.ZipFile]::CreateFromDirectory('%RELEASE%\staging_win32', '%RELEASE%\ts3websitepreview_win32.ts3_plugin')"
if errorlevel 1 goto :fail

echo.
echo === Packaging x64 ===
copy /Y "%ROOT%\ts3websitepreview\lib64\libcurl.dll" "%RELEASE%\staging_x64\plugins\ts3websitepreview\"
if errorlevel 1 goto :fail
copy /Y "%ROOT%\ts3websitepreview\lib64\libxml2.dll" "%RELEASE%\staging_x64\plugins\ts3websitepreview\"
if errorlevel 1 goto :fail
powershell -NoProfile -Command "(Get-Content '%ROOT%\ts3websitepreview\package.ini') -replace '\{PLATFORM\}', 'win64' | Set-Content -Encoding ASCII '%RELEASE%\staging_x64\package.ini'"
if errorlevel 1 goto :fail
if exist "%RELEASE%\ts3websitepreview_win64.ts3_plugin" del "%RELEASE%\ts3websitepreview_win64.ts3_plugin"
powershell -NoProfile -Command "Add-Type -Assembly 'System.IO.Compression.FileSystem'; [System.IO.Compression.ZipFile]::CreateFromDirectory('%RELEASE%\staging_x64', '%RELEASE%\ts3websitepreview_win64.ts3_plugin')"
if errorlevel 1 goto :fail

echo.
echo === Done ===
echo Output: %RELEASE%\ts3websitepreview_win32.ts3_plugin
echo Output: %RELEASE%\ts3websitepreview_win64.ts3_plugin
goto :end

:fail
echo.
echo === BUILD FAILED ===
exit /b 1

:end
endlocal
