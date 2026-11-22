#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Removes the NekoBox core service and restores normal GUI-owned core operation.
.DESCRIPTION
  Stops and deletes service "NekoBoxCore", removes %ProgramData%\NekoBoxService and
  %ProgramData%\NekoBoxService-ipc (saved profile snapshot, logs, status). Does not touch the
  NekoBox installation, the GUI's own profiles/settings or the "NekoBox AutoStart" task.
  Use -KeepData to keep the data directories (e.g. while troubleshooting).
#>
[CmdletBinding()]
param([switch]$KeepData)
$ErrorActionPreference = 'Stop'
$svcName = 'NekoBoxCore'
$base = Join-Path $env:ProgramData 'NekoBoxService'
$ipc  = Join-Path $env:ProgramData 'NekoBoxService-ipc'

$svc = Get-Service -Name $svcName -ErrorAction SilentlyContinue
if ($svc) {
    if ($svc.Status -ne 'Stopped') {
        Write-Host "Stopping service (the core closes TUN, restores DNS and routes)..."
        Stop-Service $svcName -Force
        $svc.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
    }
    & sc.exe delete $svcName | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warning "sc delete returned $LASTEXITCODE (a reboot may be needed to finish removal)" }
} else {
    Write-Host "Service $svcName is not installed."
}

# Fallback for a core that outlived the service (should not happen).
Get-Process -Name nekobox_core -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq 0 } |
    ForEach-Object { Write-Warning "nekobox_core.exe (pid $($_.Id)) still runs in session 0"; }

if (-not $KeepData) {
    foreach ($d in @($base, $ipc)) {
        if (Test-Path $d) {
            & takeown.exe /f $d /r /d y 2>&1 | Out-Null
            & icacls.exe $d /grant '*S-1-5-32-544:(OI)(CI)F' /t 2>&1 | Out-Null
            Remove-Item -Recurse -Force $d
        }
    }
    Write-Host "Removed service data."
}
Write-Host "Done. The GUI will start its own core again (set NEKOBOX_NO_SERVICE=1 to force that while the service is still installed)."
