@SETLOCAL

@REM this sets up the path for the x86 tools, but it doesn't appear to
@REM cause a problem when building for x64.
@CALL "%VS140COMNTOOLS%\..\Tools\vsvars32.bat"

@msbuild %*
