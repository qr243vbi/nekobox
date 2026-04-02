function Get-IniContent ($filePath)
{
    $ini = @{}
    $ini["General"] = @{}

    if (-not (Test-Path $filePath)) {
        return $ini
    }

    switch -regex -file $FilePath
    {
        “^\[(.+)\]” # Section
        {
            $section = $matches[1]
            $ini[$section] = @{}
            $CommentCount = 0
        }
        “^(;.*)$” # Comment
        {
            $value = $matches[1]
            $CommentCount = $CommentCount + 1
            $name = “Comment” + $CommentCount
            $ini[$section][$name] = $value
        }
        “(.+?)\s*=(.*)” # Key
        {
            $name,$value = $matches[1..2]
            $ini[$section][$name] = $value
        }
    }
    return $ini
}

function Out-IniFile($InputObject, $FilePath)
{
    $outFile = New-Item -ItemType file -Path $Filepath -Force
    foreach ($i in $InputObject.keys)
    {
        if (!($($InputObject[$i].GetType().Name) -eq “Hashtable”))
        {
            #No Sections
            Add-Content -Path $outFile -Value “$i=$($InputObject[$i])”
        } else {
            #Sections
            Add-Content -Path $outFile -Value “[$i]”
            Foreach ($j in ($InputObject[$i].keys | Sort-Object))
            {
                if ($j -match “^Comment[\d]+”) {
                    Add-Content -Path $outFile -Value “$($InputObject[$i][$j])”
                } else {
                    Add-Content -Path $outFile -Value “$j=$($InputObject[$i][$j])”
                }

            }
            Add-Content -Path $outFile -Value “”
        }
    }
}



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
  url           = '@URL_x86@'
  url64bit      = '@URL_x64@'
  softwareName  = 'nekobox*'
  checksum      = '@SHA_x86@'
  checksumType  = 'sha256'
  checksum64    = '@SHA_x64@'
  checksumType64= 'sha256'
  silentArgs    = "/S /D=C:\tools\NekoBox"
  validExitCodes= @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs 

$nekobox_path = Get-AppInstallLocation $packageArgs.softwareName


if ([string]::IsNullOrEmpty($appPath)) {

} else {

if (Test-Path "$nekobox_path" -PathType Container) {
    $global_ini_path = Join-Path $nekobox_path "global.ini"

    $data = Get-IniContent "$global_ini_path"
    $data["General"]["chocolatey_package"] = "true";
    Out-IniFile $data "$global_ini_path"

}

}




if ($programRunning -and (Test-Path $programRunning)) {
  Write-Host "Please reopen NekoBox to continue using."
}
