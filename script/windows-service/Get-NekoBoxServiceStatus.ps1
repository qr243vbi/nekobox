<#
.SYNOPSIS  Diagnostics for the NekoBox core service (run in an elevated prompt for full output).
#>
$svcName = 'NekoBoxCore'
$base = Join-Path $env:ProgramData 'NekoBoxService'
$ipc  = Join-Path $env:ProgramData 'NekoBoxService-ipc'

Write-Host "== Service ==" -ForegroundColor Cyan
$svc = Get-Service $svcName -ErrorAction SilentlyContinue
if ($svc) { $svc | Format-List Name, Status, StartType } else { Write-Host "not installed" }

Write-Host "== Status file ==" -ForegroundColor Cyan
$st = Join-Path $ipc 'status.json'
if (Test-Path $st) {
    $j = Get-Content $st -Raw | ConvertFrom-Json
    $j | Format-List
    $age = [int]([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() - $j.updated)
    Write-Host "heartbeat age: ${age}s (stale if > 30)"
} else { Write-Host "missing" }

Write-Host "== Processes ==" -ForegroundColor Cyan
$p = Get-CimInstance Win32_Process -Filter "Name='nekobox_core.exe'" | Select-Object ProcessId, SessionId, CommandLine
$p | Format-Table -AutoSize
$n = @($p).Count
if ($n -gt 1) { Write-Warning "More than one nekobox_core.exe is running ($n). Only the service's core should exist." }

Write-Host "== TUN / adapters ==" -ForegroundColor Cyan
Get-NetAdapter -IncludeHidden -ErrorAction SilentlyContinue | Where-Object { $_.InterfaceDescription -match 'Wintun|sing-tun|nekobox' -or $_.Name -match 'nekobox|tun' } |
    Format-Table Name, InterfaceDescription, Status -AutoSize

Write-Host "== Last log lines ==" -ForegroundColor Cyan
$log = Join-Path $base 'logs\service.log'
if (Test-Path $log) { Get-Content $log -Tail 40 } else { Write-Host "no log (run as Administrator?)" }

Write-Host "== SCM events (last 10) ==" -ForegroundColor Cyan
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Service Control Manager'} -MaxEvents 40 -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'NekoBox' } | Select-Object -First 10 TimeCreated, Message | Format-List
