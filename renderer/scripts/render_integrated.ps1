param(
  [ValidateRange(1,4096)][int]$Samples = 64,
  [ValidateRange(16,4096)][int]$Size = 128,
  [ValidateRange(1,64)][int]$Threads = 1,
  [string]$Executable,
  [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$rendererRoot = Split-Path -Parent $PSScriptRoot
if (-not $Executable) { $Executable = Join-Path $rendererRoot 'out\build\x64-Release\pathtracer.exe' }
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'Build first, or pass -Executable with the pathtracer.exe path.' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $rendererRoot 'renders\integrated' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
foreach ($mode in @('baseline','vacuum','absorption','fog','cloud-isotropic','cloud-hg')) {
  $renderArgs = @('-s', "$Samples", '-l', '4', '-t', "$Threads", '-m', '6', '-a', '0', '0',
                  '-r', "$Size", "$Size", '-c', (Join-Path $rendererRoot 'scenes\volume\cornell_demo.cam'),
                  '-f', (Join-Path $OutputDirectory ($mode + '.png')))
  $sidecar = switch ($mode) {
    'vacuum' { 'vacuum.vol' }
    'absorption' { 'cornell_absorption.vol' }
    'fog' { 'cornell_fog.vol' }
    'cloud-isotropic' { 'cornell_cloud_isotropic.vol' }
    'cloud-hg' { 'cornell_cloud.vol' }
  }
  if ($sidecar) { $renderArgs += @('-v', (Join-Path $rendererRoot ('scenes\volume\' + $sidecar))) }
  $renderArgs += (Join-Path $rendererRoot 'scenes\volume\cornell_demo.dae')
  & $Executable @renderArgs
  if ($LASTEXITCODE -ne 0) { throw "$mode render failed with code $LASTEXITCODE" }
  Write-Host "Saved $mode in $OutputDirectory"
}
