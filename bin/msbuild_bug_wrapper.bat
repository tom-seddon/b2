@echo off

setlocal

REM workaround for https://github.com/dotnet/msbuild/issues/5726
REM
REM This is not really a proper solution. It looks like any variable could
REM cause, so, really, the entire environment needs fixing up. But it seems
REM GNU Make renames Path to PATH specifically, leaving everything else
REM the same, so, fingers crossed, this'll cover it for b2 building
REM purposes.

set _MSBUILD_BUG_WRAPPER_TEMP=%PATH%
set PATH=
set Path=%_MSBUILD_BUG_WRAPPER_TEMP%
set _MSBUILD_BUG_WRAPPER_TEMP=
%*
