#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Installs (or updates) the NekoBox core as a Windows service that starts before logon.

.DESCRIPTION
  * Registers service "NekoBoxCore" (LocalSystem, automatic start, restart on failure).
  * Creates %ProgramData%\NekoBoxService (SYSTEM + Administrators only) and writes service.json.
  * Allows the given Windows account to talk to the core (ACL on the IPC socket directory).
  * Does NOT touch the existing "NekoBox AutoStart" scheduled task or any user setting.
  * Idempotent: run it again after replacing the NekoBox files.

  The service has nothing to start until the GUI has started a profile once (the GUI
  attaches to the service and the service saves that profile). See README-service.md.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = 'E:\applications\nekobox',
    [string]$User       = $env:USERNAME,
    [string]$RulesetDir = '',
    [string[]]$ExtraCoreDirs = @(),
    [switch]$DebugLog
)
$ErrorActionPreference = 'Stop'
$svcName = 'NekoBoxCore'
$core    = Join-Path $InstallDir 'nekobox_core.exe'
$base    = Join-Path $env:ProgramData 'NekoBoxService'
$ipc     = Join-Path $env:ProgramData 'NekoBoxService-ipc'

if (-not (Test-Path $core)) { throw "nekobox_core.exe not found: $core" }

# A stock core would treat the 'service' argument as an ordinary run and start a stray core.
# Refuse to register it: look for the marker string compiled into service-capable builds.
$null = & findstr.exe /m /c:"NEKOBOX-SERVICE-MODE-V1" $core
if ($LASTEXITCODE -ne 0) {
    throw "$core does not contain service mode (marker not found). Install the patched build first."
}

# Resolve the account to a SID (works for local and domain accounts).
try {
    $sid = (New-Object System.Security.Principal.NTAccount($User)).Translate([System.Security.Principal.SecurityIdentifier]).Value
} catch { throw "Cannot resolve Windows account '$User': $_" }

# Stop an existing instance before touching files.
$existing = Get-Service -Name $svcName -ErrorAction SilentlyContinue
if ($existing -and $existing.Status -ne 'Stopped') {
    Write-Host "Stopping existing service..."
    Stop-Service $svcName -Force
    $existing.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
}

# Private data directory: SYSTEM + Administrators only, no inheritance.
New-Item -ItemType Directory -Force -Path $base | Out-Null
& icacls.exe $base /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' | Out-Null
if ($LASTEXITCODE -ne 0) { throw "icacls failed on $base" }

# Configuration (contains no secrets: only SIDs and paths).
$cfgPath = Join-Path $base 'service.json'
$cfg = [ordered]@{
    allowed_users    = @($sid)
    ruleset_dir      = $RulesetDir
    extra_core_dirs  = @($ExtraCoreDirs)
    debug            = [bool]$DebugLog
    watchdog_seconds = 10
}
if (Test-Path $cfgPath) {
    # keep users added by hand, merge the requested one
    try {
        $old = Get-Content $cfgPath -Raw | ConvertFrom-Json
        $cfg.allowed_users = @($old.allowed_users + $sid | Select-Object -Unique)
        if (-not $RulesetDir -and $old.ruleset_dir) { $cfg.ruleset_dir = $old.ruleset_dir }
        if (-not $ExtraCoreDirs.Count -and $old.extra_core_dirs) { $cfg.extra_core_dirs = @($old.extra_core_dirs) }
    } catch { Write-Warning "Existing service.json unreadable, overwriting: $_" }
}
# UTF-8 without BOM (Set-Content -Encoding UTF8 adds a BOM in Windows PowerShell 5.1)
[System.IO.File]::WriteAllText($cfgPath, ($cfg | ConvertTo-Json -Depth 4), (New-Object System.Text.UTF8Encoding($false)))

# Register / update the service.
$binPath = "`"$core`" service run"
if (-not $existing) {
    New-Service -Name $svcName -BinaryPathName $binPath -DisplayName 'NekoBox Core' `
        -Description 'Runs the NekoBox core (sing-box, TUN) before user logon. The NekoBox GUI attaches to it.' `
        -StartupType Automatic | Out-Null
} else {
    $s = Get-CimInstance Win32_Service -Filter "Name='$svcName'"
    $r = $s | Invoke-CimMethod -MethodName Change -Arguments @{ PathName = $binPath; StartMode = 'Automatic' }
    if ($r.ReturnValue -ne 0) { throw "Changing the service failed (code $($r.ReturnValue))" }
}
& sc.exe config $svcName depend= Tcpip | Out-Null
& sc.exe failure $svcName reset= 86400 actions= restart/5000/restart/5000/restart/30000 | Out-Null
& sc.exe failureflag $svcName 1 | Out-Null   # also restart on non-zero exit code

Start-Service $svcName
Write-Host "Service started. Waiting for status..."
$status = Join-Path $ipc 'status.json'
for ($i = 0; $i -lt 30 -and -not (Test-Path $status); $i++) { Start-Sleep -Milliseconds 500 }
if (Test-Path $status) { Get-Content $status } else { Write-Warning "No status file yet. Check $base\logs\service.log" }

$task = Get-ScheduledTask -TaskName 'NekoBox AutoStart' -ErrorAction SilentlyContinue
if ($task) { Write-Host "Scheduled task 'NekoBox AutoStart' found and left untouched (the GUI will attach to the service)." }
Write-Host @"

Next steps:
  1. Start the NekoBox GUI, enable TUN and connect your profile once. The log shows
     "[Service] Attached to the NekoBox core service". The service saves that profile.
  2. Reboot. The VPN is up before the logon screen. Check with Get-NekoBoxServiceStatus.ps1.
  3. Rollback: Uninstall-NekoBoxService.ps1
"@
