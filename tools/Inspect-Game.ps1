[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot
)
$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$exePath = Join-Path $resolvedRoot 'bin\x64\Cyberpunk2077.exe'
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Cyberpunk2077.exe was not found under the supplied game root: $resolvedRoot"
}
$exe = Get-Item -LiteralPath $exePath
[pscustomobject]@{
    Component = 'Cyberpunk2077.exe'
    Present = $true
    Version = $exe.VersionInfo.FileVersion
    Path = $exe.FullName
}
$components = [ordered]@{
    RED4ext = 'red4ext\RED4ext.dll'
    CET = 'bin\x64\plugins\cyber_engine_tweaks.asi'
    RedscriptCompiler = 'engine\tools\scc.exe'
    Codeware = 'red4ext\plugins\Codeware\Codeware.dll'
    TweakXL = 'red4ext\plugins\TweakXL\TweakXL.dll'
    ArchiveXL = 'red4ext\plugins\ArchiveXL\ArchiveXL.dll'
}
foreach ($entry in $components.GetEnumerator()) {
    $componentPath = Join-Path $resolvedRoot $entry.Value
    $present = Test-Path -LiteralPath $componentPath -PathType Leaf
    $version = if ($present) { (Get-Item -LiteralPath $componentPath).VersionInfo.FileVersion } else { $null }
    [pscustomobject]@{
        Component = $entry.Key
        Present = $present
        Version = $version
        Path = $componentPath
    }
}
# Presence is diagnostic only: nonstandard layouts and versions without file metadata
# require checking the corresponding mod log or release manifest.
