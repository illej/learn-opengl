@echo off

if not defined MSVC_HOST (
    call "%USERPROFILE%\code\msvc\setup.bat"
)

if not exist SDL2.dll call sdl.bat
if not exist glew32.dll call glew.bat

set defines=-DDEBUG

set libs=Shell32.lib SDL2.lib SDL2main.lib glew32.lib glew32s.lib OpenGL32.lib
set cflags=%defines% /I include
set ldflags=/link /subsystem:console /libpath:lib\x64 %libs%
set source=main.c

cl %cflags% %source% %ldflags%

ctags -R .
