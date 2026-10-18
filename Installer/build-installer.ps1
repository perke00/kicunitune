<#
.SYNOPSIS  Builds KITSUNE Auto Tune (Release), runs tests, and packages the Windows installer.
.NOTES     Requires: Visual Studio 2022 (C++ workload), CMake >= 3.22, Git, Inno Setup 6 (ISCC.exe).
#>
param(
  [string]$Version = "0.1.0",
  [switch]$SkipTests
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $root
try {
  cmake --preset vs2022
  cmake --build --preset release --parallel
  if (-not $SkipTests) { ctest --preset release }

  $bundle = Join-Path $root "build\KitsuneAutoTune_artefacts\Release\VST3\KITSUNE Auto Tune.vst3"
  if (-not (Test-Path $bundle)) { throw "VST3 bundle not found: $bundle" }

  $iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
  if (-not $iscc) {
    $c = @("$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ }
    $iscc = $c | Select-Object -First 1
  }
  if (-not $iscc) { throw "Inno Setup 6 not found. Install from https://jrsoftware.org/isdl.php" }

  & $iscc "/DMyVersion=$Version" "/DBundleDir=$bundle" (Join-Path $PSScriptRoot "installer.iss")
  Write-Host "Installer: $(Join-Path $PSScriptRoot 'Output')"
} finally { Pop-Location }
