param(
  [ValidateSet('fog','absorption','vacuum','baseline')][string]$Mode = 'fog',
  [ValidateRange(1,4096)][int]$Samples = 64,
  [ValidateRange(16,4096)][int]$Size = 256,
  [string]$Executable
)
$ErrorActionPreference = 'Stop'
$rendererRoot = Split-Path -Parent $PSScriptRoot
if (-not $Executable) {
  $Executable = Join-Path $rendererRoot 'out\build\x64-Release\pathtracer.exe'
}
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
  throw "Build x64-Release first, or pass -Executable with the pathtracer.exe path."
}
$outputDirectory = Join-Path $rendererRoot 'renders\part-a'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$output = Join-Path $outputDirectory ($Mode + '.png')
$renderArgs = @('-s', "$Samples", '-l', '4', '-t', '1', '-m', '6', '-a', '0', '0',
                '-r', "$Size", "$Size", '-f', $output)
if ($Mode -ne 'baseline') {
  $config = switch ($Mode) {
    'fog' { 'cornell_fog.vol' }
    'absorption' { 'cornell_absorption.vol' }
    'vacuum' { 'vacuum.vol' }
  }
  $renderArgs += @('-v', (Join-Path $rendererRoot ('scenes\volume\' + $config)))
}
$renderArgs += @('-c', (Join-Path $rendererRoot 'scenes\volume\cornell_demo.cam'))
$renderArgs += (Join-Path $rendererRoot 'scenes\volume\cornell_demo.dae')
& $Executable @renderArgs
if ($LASTEXITCODE -ne 0) { throw "Render failed with code $LASTEXITCODE" }
Write-Host "Saved render: $output"
