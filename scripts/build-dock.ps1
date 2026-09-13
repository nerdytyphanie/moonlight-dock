# Moonlight Dock additions, 2026. GPL-3.0-or-later; see LICENSE.
param([string]$QtBin = 'C:\Qt\6.10.3\msvc2022_64\bin')
$ErrorActionPreference = 'Stop'
$source = Split-Path $PSScriptRoot -Parent
$env:BUILD_ROOT = Join-Path $source 'build'
$env:BUILD_CONFIG = 'release'
$env:BUILD_FOLDER = Join-Path $env:BUILD_ROOT 'build-x64-release'
$env:DEPLOY_FOLDER = Join-Path $env:BUILD_ROOT 'deploy-x64-release'
$env:INSTALLER_FOLDER = Join-Path $env:BUILD_ROOT 'installer-x64-release'
$version = '6.1.6-dock.1'
# Rebuild only the generated deployment tree; never harvest a previous portable.dat
# or its app-local VC runtime into the full installer.
$deploy = [IO.Path]::GetFullPath($env:DEPLOY_FOLDER)
if (!$deploy.StartsWith([IO.Path]::GetFullPath($env:BUILD_ROOT) + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid deployment path.' }
if (Test-Path -LiteralPath $deploy) { Remove-Item -LiteralPath $deploy -Recurse -Force }
$vs = & "$PSScriptRoot\vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ tools are required.' }
foreach ($folder in @($env:BUILD_FOLDER, $env:DEPLOY_FOLDER, $env:INSTALLER_FOLDER)) {
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
}
function Check-Exit([string]$step) { if ($LASTEXITCODE) { throw "$step failed ($LASTEXITCODE)." } }
$compile = Join-Path $env:BUILD_ROOT 'compile-dock.cmd'
@"
@echo off
call "$vs\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d "$env:BUILD_FOLDER"
"$QtBin\qmake.exe" -r "$source\moonlight-qt.pro"
if errorlevel 1 exit /b 1
pushd "$env:BUILD_FOLDER\app"
"$PSScriptRoot\jom.exe" -f Makefile.Release clean
if errorlevel 1 exit /b 1
popd
if errorlevel 1 exit /b 1
"$PSScriptRoot\jom.exe" release -j 8
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 "$source\tests\dockwindow.cpp" /Fe:"$env:BUILD_ROOT\dockwindow-test.exe" /Fo:"$env:BUILD_ROOT\dockwindow-test.obj" user32.lib
if errorlevel 1 exit /b 1
"$env:BUILD_ROOT\dockwindow-test.exe"
"@ | Set-Content -LiteralPath $compile -Encoding ascii
& $compile
Check-Exit 'Compile and native checks'
Copy-Item "$source\libs\windows\lib\x64\*.dll" $env:DEPLOY_FOLDER
Copy-Item "$env:BUILD_FOLDER\AntiHooking\release\AntiHooking.dll" $env:DEPLOY_FOLDER
Copy-Item "$source\app\SDL_GameControllerDB\gamecontrollerdb.txt" $env:DEPLOY_FOLDER
& "$QtBin\windeployqt.exe" --dir $env:DEPLOY_FOLDER --release --qmldir "$source\app\gui" --no-opengl-sw --no-compiler-runtime --no-sql --no-system-d3d-compiler --no-system-dxc-compiler --skip-plugin-types qmltooling,generic --no-ffmpeg "$env:BUILD_FOLDER\app\release\Moonlight.exe"
Check-Exit 'Qt deployment'
Copy-Item "$source\LICENSE" $env:DEPLOY_FOLDER
Copy-Item "$source\README.md" (Join-Path $env:DEPLOY_FOLDER 'MoonlightDock-README.md')
'{"protocol":1,"version":"6.1.6-dock.1"}' | Set-Content (Join-Path $env:DEPLOY_FOLDER 'moonlight-dock.json') -Encoding ascii
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild -Restore "$source\wix\Moonlight\Moonlight.wixproj" /p:Configuration=Release /p:Platform=x64 "/p:MSBuildProjectExtensionsPath=$env:BUILD_FOLDER\" /nologo
Check-Exit 'MSI packaging'
& $msbuild -Restore "$source\wix\MoonlightSetup\MoonlightSetup.wixproj" /p:Configuration=Release /p:Platform=x86 /p:DefineConstants=DockX64Only "/p:MSBuildProjectExtensionsPath=$env:BUILD_ROOT\bundle\" /nologo
Check-Exit 'Setup EXE packaging'
Copy-Item "$env:INSTALLER_FOLDER\MoonlightSetup.exe" "$env:INSTALLER_FOLDER\MoonlightDockSetup-x64-$version.exe"
Copy-Item "$env:BUILD_FOLDER\Moonlight.msi" "$env:INSTALLER_FOLDER\MoonlightDock-x64-$version.msi"
Copy-Item "$env:BUILD_FOLDER\app\release\Moonlight.exe" $env:DEPLOY_FOLDER
$crt = & "$PSScriptRoot\vswhere.exe" -latest -products '*' -find 'VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT'
if (!$crt) { throw 'Visual C++ runtime files not found.' }
Copy-Item (Join-Path (@($crt)[-1]) '*.dll') $env:DEPLOY_FOLDER
Set-Content (Join-Path $env:DEPLOY_FOLDER 'portable.dat') '' -Encoding ascii
# Explicit deployment directory only: tests, build products and source are never packaged.
$zip = Join-Path $env:INSTALLER_FOLDER "MoonlightDockPortable-x64-$version.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
& 'C:\Program Files\7-Zip\7z.exe' a -tzip $zip "$env:DEPLOY_FOLDER\*"
Check-Exit 'Portable ZIP packaging'
Get-FileHash "$env:INSTALLER_FOLDER\MoonlightDock*" -Algorithm SHA256
