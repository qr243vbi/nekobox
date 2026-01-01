package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"time"
)

func LaunchCmd(cmd *exec.Cmd) error {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
}

func Launch(Path string, Args ...string) error {
	cmd := exec.Command(Path, Args...)
	return LaunchCmd(cmd)
}

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}
	version := flag.String("version", "", "version")
	chocolatey_source := flag.String("chocolatey_source", "", "install with chocolatey from source")
	winget_install := flag.Bool("winget_install", false, "install with winget")
	verbose := flag.Bool("verbose", false, "verbose mode")
	name := flag.String("name", "nekobox", "software name")
	// Parse the flags
	flag.Parse()
	// Get the positional arguments
	args := flag.Args()
	wd := args[1]
	box := args[0]
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd, "box: ", box)
	time.Sleep(2 * time.Second)
	// 1. update files
	LaunchInstaller(box, wd, *version, *chocolatey_source, *winget_install, *verbose, *name)
	// 2. start
	os.Chdir(wd)
	exec.Command("./nekobox.exe", args[2:]...).Start()
}

func LaunchInstaller(updatePackagePath string, installPath string, version string, chocolatey_source string, winget_install bool, verbose bool, name string) {
	if winget_install {
		Launch("winget", "install", "--version", version, updatePackagePath)
	} else {
		if chocolatey_source != "" {
			run_chocolatey(version, chocolatey_source, name)
		}
		Launch(updatePackagePath, "/S", "/UNPACK=1", "/D="+installPath)
	}
}

func run_chocolatey(version string, source string, name string) {
	if source != "" {
		command := exec.Command("powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", `
$source="`+source+`"
$version="`+version+`"
$name="`+name+`"
$IsAdmin = ([Security.Principal.WindowsPrincipal] `+"`"+`
[Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) ;
    $psi = New-Object System.Diagnostics.ProcessStartInfo ;
    $psi.FileName = "choco" ;
    $psi.Arguments = "install `+"`"+`"$name`+"`"+`" --version=`+"`"+`"$version`+"`"+`" --source=`+"`"+`"$source`+"`"+`" --skip-scripts" ;

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
		LaunchCmd(command)
	}
}
