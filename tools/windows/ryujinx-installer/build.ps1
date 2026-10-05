<#
.SYNOPSIS
    Builds the Ryujinx installer (ryujinx-<version>-setup.exe) with Inno Setup.

.DESCRIPTION
    Takes an unmodified Ryujinx build (the folder with Ryujinx.exe in it, or a zip of it),
    reads its version from Ryujinx.exe, and compiles ryujinx-setup.iss.
    Output goes to tools\windows\ryujinx-installer\build\ (gitignored).

    Needs Inno Setup 6 (https://jrsoftware.org/isinfo.php). ISCC.exe is found on PATH or in
    its default install folders.

.EXAMPLE
    .\build.ps1 -Ryujinx "C:\Emulators\ryujinx-1.1.1380-win_x64\publish"

.EXAMPLE
    .\build.ps1 -Ryujinx .\ryujinx-1.1.1380-win_x64.zip -Sha256 54293afe00e2e8b3afb7c162ba8d4b7e2573ec2922350cde77af63e47a01f622
#>
[CmdletBinding()]
param(
    # Folder containing Ryujinx.exe, or a zip of that folder's contents.
    [Parameter(Mandatory)][string]$Ryujinx,

    # Optional: expected SHA-256 of the zip (CI passes this so a swapped download can't be packaged).
    [string]$Sha256,

    # Optional: override the version read from Ryujinx.exe.
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$here  = $PSScriptRoot
$build = Join-Path $here 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null

# 1. Get a folder with Ryujinx.exe in it.
$src = (Resolve-Path $Ryujinx).Path
if ((Get-Item $src).PSIsContainer) {
    $ryuDir = $src
} else {
    if ($Sha256) {
        $actual = (Get-FileHash -Algorithm SHA256 $src).Hash
        if ($actual -ne $Sha256) { throw "SHA-256 mismatch for $src`n  expected $Sha256`n  got      $actual" }
    }
    $ryuDir = Join-Path $build 'ryujinx'
    if (Test-Path $ryuDir) { Remove-Item -Recurse -Force $ryuDir }
    Expand-Archive -LiteralPath $src -DestinationPath $ryuDir
}
$exe = Get-ChildItem -Path $ryuDir -Filter Ryujinx.exe -Recurse | Select-Object -First 1
if (-not $exe) { throw "No Ryujinx.exe found in $ryuDir" }
$ryuDir = $exe.DirectoryName

# 2. Version from the exe (FileVersion is like 1.1.1380.0).
if (-not $Version) {
    $fv = $exe.VersionInfo.FileVersion
    if (-not $fv) { throw "Couldn't read the version from $($exe.FullName); pass -Version." }
    $Version = ($fv -split '\.')[0..2] -join '.'
}
Write-Host "Packaging Ryujinx $Version from $ryuDir"

# 3. Find the Inno Setup compiler.
$iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { throw 'ISCC.exe not found. Install Inno Setup 6 from https://jrsoftware.org/isinfo.php' }

# 4. Compile.
& $iscc "/DRyujinxDir=$ryuDir" "/DRyujinxVersion=$Version" "/O$build" (Join-Path $here 'ryujinx-setup.iss')
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

$out = Join-Path $build "ryujinx-$Version-setup.exe"
$hash = (Get-FileHash -Algorithm SHA256 $out).Hash
Write-Host "`nBuilt $out`nSHA-256 $hash"
