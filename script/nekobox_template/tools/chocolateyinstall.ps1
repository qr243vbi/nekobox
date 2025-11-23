$ErrorActionPreference = 'Stop'
$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"

$packageArgs = @{
  packageName   = $env:ChocolateyPackageName
  unzipLocation = $toolsDir
  fileType      = 'exe'
  url           = '@URL_x86@'
  url64bit      = '@URL_x64@'
  softwareName  = 'nekobox*'
  checksum      = '@SHA_x86@'
  checksumType  = 'sha256'
  checksum64    = '@SHA_x64@'
  checksumType64= 'sha256'
  silentArgs    = "/S /D=C:\tools\nekobox"
  validExitCodes= @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs 
