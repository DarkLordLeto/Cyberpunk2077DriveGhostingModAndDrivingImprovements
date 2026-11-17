# Builds the Release plugin, runs the test suites, then installs the DLL with a
# hash-verified backup. Close the game first. Run from any directory:
#   powershell -ExecutionPolicy Bypass -File tools\Build-Install.ps1
[CmdletBinding()]
param(
    [string]$GameRoot = 'D:\SteamLibrary\steamapps\common\Cyberpunk 2077'
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$build = Join-Path $project 'build'
$cmakeDir = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $cmakeDir 'cmake.exe'
$ctest = Join-Path $cmakeDir 'ctest.exe'
if (-not (Test-Path -LiteralPath $cmake)) { $cmake = 'cmake'; $ctest = 'ctest' }

if (Get-Process -Name 'Cyberpunk2077' -ErrorAction SilentlyContinue) {
    throw 'Cyberpunk 2077 is running. Close the game and run this script again.'
}

& $cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed. Nothing was installed.' }
& $ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'A test suite failed. Nothing was installed.' }

$version = (Select-String -LiteralPath (Join-Path $project 'CMakeLists.txt') -Pattern 'VERSION (\d+\.\d+\.\d+) LANGUAGES').Matches[0].Groups[1].Value
$built = Join-Path $build 'Release\DriveNoCollision.dll'
$pluginDir = Join-Path $GameRoot 'red4ext\plugins\DriveNoCollision'
$installed = Join-Path $pluginDir 'DriveNoCollision.dll'
$artifacts = Join-Path $project 'artifacts'
$backup = $null
if (Test-Path -LiteralPath $installed) {
    $backup = Join-Path $artifacts ("before-$version-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.dll')
    Copy-Item -LiteralPath $installed -Destination $backup
}
New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
Copy-Item -LiteralPath $built -Destination $installed -Force
$builtHash = (Get-FileHash -LiteralPath $built -Algorithm SHA256).Hash
$installedHash = (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash
if ($builtHash -ne $installedHash) { throw 'Installed DLL hash does not match the built DLL.' }
[ordered]@{ version = $version; sha256 = $installedHash; installed = $installed; backup = $backup } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $artifacts "install-$version.json") -Encoding UTF8
Write-Host "Installed DriveNoCollision $version ($installedHash)"
