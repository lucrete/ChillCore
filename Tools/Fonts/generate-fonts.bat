@echo off
REM Generate MSDF font atlases for Roboto weights using msdf-atlas-gen.
REM Outputs .png atlas + .json metrics to Code\App\Data\Fonts\
REM Run from project root or this script's directory.

setlocal

set TOOL_DIR=%~dp0msdf-atlas-gen-1.3-win64
set FONT_SRC=%~dp0..\..\ContentSource\Fonts\Roboto\static
set OUTPUT_DIR=%~dp0..\..\Code\App\Data\Fonts

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

set COMMON_ARGS=-type msdf -format png -size 48 -pxrange 4

echo Generating robotoLight...
"%TOOL_DIR%\msdf-atlas-gen.exe" -font "%FONT_SRC%\Roboto-Light.ttf" %COMMON_ARGS% -imageout "%OUTPUT_DIR%\robotoLight.png" -json "%OUTPUT_DIR%\robotoLight.json"

echo Generating robotoRegular...
"%TOOL_DIR%\msdf-atlas-gen.exe" -font "%FONT_SRC%\Roboto-Regular.ttf" %COMMON_ARGS% -imageout "%OUTPUT_DIR%\robotoRegular.png" -json "%OUTPUT_DIR%\robotoRegular.json"

echo Generating robotoMedium...
"%TOOL_DIR%\msdf-atlas-gen.exe" -font "%FONT_SRC%\Roboto-Medium.ttf" %COMMON_ARGS% -imageout "%OUTPUT_DIR%\robotoMedium.png" -json "%OUTPUT_DIR%\robotoMedium.json"

echo Generating robotoBold...
"%TOOL_DIR%\msdf-atlas-gen.exe" -font "%FONT_SRC%\Roboto-Bold.ttf" %COMMON_ARGS% -imageout "%OUTPUT_DIR%\robotoBold.png" -json "%OUTPUT_DIR%\robotoBold.json"

echo Done.
endlocal
