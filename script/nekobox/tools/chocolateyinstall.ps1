$ErrorActionPreference = 'Stop'
$toolsDir   = "$(Split-Path -parent $MyInvocation.MyCommand.Definition)"

$packageArgs = @{
  packageName   = $env:ChocolateyPackageName
  unzipLocation = $toolsDir
  fileType      = 'exe'
  url           = '@URL_x86@'
  url64bit      = 'https://github.com/qr243vbi/nekobox/releases/download/5.9.18/nekobox-5.9.18-windows64-installer.exe'
  softwareName  = 'nekobox*'
  checksum      = '@SHA_x86@'
  checksumType  = 'sha256'
  checksum64    = ''
  checksumType64= 'sha256'
  silentArgs    = "/S"
  validExitCodes= @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs 


$nekobox_path = Join-Path $env:ProgramFiles "nekobox"

if (Test-Path "$nekobox_path" -PathType Container) {
    $global_ini_path = Join-Path $nekobox_path "global.ini"
@"
[General]
chocolatey_package=true
"@ | Set-Content "$global_ini_path"

} 

