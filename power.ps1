$IsAdmin = ([Security.Principal.WindowsPrincipal] `
[Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) ;
    $psi = New-Object System.Diagnostics.ProcessStartInfo ;
    $psi.FileName = "choco" ;
    $psi.Arguments = "install nekobox --source=`"$pwd`" --skip-scripts" ;

if (-not $IsAdmin) {
    Write-Host "Not elevated. Launching as Administrator..." ;
    $psi.Verb = "runas" ;
}

try {
	$proc = [System.Diagnostics.Process]::Start($psi) ;
	if ($proc) { 
		$proc.WaitForExit() ; 
	}
} catch {
    Write-Host "Installation failed." ;
}

exit ;

