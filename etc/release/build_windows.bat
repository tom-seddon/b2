@SETLOCAL

@REM Work around msbuild getting confused by the path environment
@REM variable not being spelled PATH (case-sensitive). See
@REM https://github.com/dotnet/msbuild/issues/5726
@REM
@REM 

@set _PATH=%PATH%
@set PATH=
@set PATH=%_PATH%
@set _PATH=

@REM this sets up the path for the x86 tools, but it doesn't appear to
@REM cause a problem when building for x64.
@REM @CALL "%VS140COMNTOOLS%\..\Tools\vsvars32.bat"

@msbuild %*
