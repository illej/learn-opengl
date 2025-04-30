@echo off

set VERSION=2.2.0

echo installing glew
echo   downloading
for /f "tokens=1,* delims=:" %%A in ('curl -ks https://api.github.com/repos/nigels-com/glew/releases/latest ^| find "browser_download_url"') do (
    echo %%B | findstr "glew-%VERSION%-win32.zip" > nul
    if not errorlevel 1 (
        curl -kL --show-error --progress-bar %%B -o glew.zip
    )
)

echo   extracting
tar -xf glew.zip

echo   copying files
robocopy glew-%VERSION%\include\GL include\GL > nul
robocopy glew-%VERSION%\lib\Release\x64 lib\x64 > nul
copy glew-%VERSION%\bin\Release\x64\glew32.dll .

echo   cleaning up
rmdir /Q /S glew-%VERSION% > nul
del glew.zip

echo done
