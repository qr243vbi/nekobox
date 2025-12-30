$ErrorActionPreference = 'Stop'
$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"

$process = Get-Process "nekobox*" -ea 0

if ($process) {
  $processPath = $process | Where-Object { $_.Path } | Select-Object -First 1 -ExpandProperty Path
  Write-Host "Found Running instance of NekoBox. Will stop during install..."
#  $process | Stop-Process
  $programRunning = $processPath
}

$packageArgs = @{
  packageName   = $env:ChocolateyPackageName
  unzipLocation = $toolsDir
  fileType      = 'exe'
#  url           = '@URL_x86@'
  url64bit      = '@URL_x64@'
  softwareName  = 'nekobox*'
#  checksum      = '@SHA_x86@'
#  checksumType  = 'sha256'
  checksum64    = '@SHA_x64@'
  checksumType64= 'sha256'
  silentArgs    = "/S /D=C:\tools\NekoBox"
  validExitCodes= @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs 

$nekobox_path = Get-AppInstallLocation $packageArgs.softwareName

if (Test-Path "$nekobox_path" -PathType Container) {
    $global_ini_path = Join-Path $nekobox_path "global.ini"
@"
[General]
chocolatey_package=true
"@ | Set-Content "$global_ini_path"

} 

if ($programRunning -and (Test-Path $programRunning)) {
  Write-Host "Please reopen NekoBox to continue using."
}
