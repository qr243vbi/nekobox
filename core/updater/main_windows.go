package main

import (
	"encoding/json"
	"encoding/hex"
	"os/exec"
	"fmt"
)

func run_json(text string){
	var m map[string]interface{}
	text2, err := hex.DecodeString(text);
	
	if err != nil { 
		panic(err)
	}
	
	data := []byte(text2)
	err = json.Unmarshal(data, &m); 
	
	if err != nil { 
		panic(err)
	}
	
	version := m["version"].(string);
	source := m["chocolatey_source"].(string);
	
	if (source != ""){
		command := exec.Command("powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", `
$source="`+source+`"
$version="`+version+`"
$IsAdmin = ([Security.Principal.WindowsPrincipal] `+"`"+`
[Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) ;
    $psi = New-Object System.Diagnostics.ProcessStartInfo ;
    $psi.FileName = "choco" ;
    $psi.Arguments = "install nekobox --version=`+"`"+`"$version`+"`"+`" --source=`+"`"+`"$source`+"`"+`" --skip-scripts" ;

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
`)
		fmt.Println("<<<<Run command>>>>")
		fmt.Println(command)
		fmt.Println("<<<>>>")
		command.Run()
	}
}