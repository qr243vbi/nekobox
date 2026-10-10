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
	log.Println(Path, Args)
	cmd := exec.Command(Path, Args...)
	return LaunchCmd(cmd)
}

//func InstallVcRedist() {

//}

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}
	log.Println(os.Args)

	version := flag.String("version", "", "version")
	chocolatey_source := flag.String("chocolatey_source", "", "install with chocolatey from source")
	winget_install := flag.Bool("winget_install", false, "install with winget")
	verbose := flag.Bool("verbose", false, "verbose mode")
	name := flag.String("name", "nekobox", "software name")

	//	nsis_installer_mode := flag.Bool("kill_processes", false, "Kill Processes from directory")
	//	install_vcredist := flag.Bool("install_vcredist", false, "Install VcRedist")
	// Parse the flags
	flag.Parse()

	//	if *install_vcredist {
	//		InstallVcRedist()
	//	}

	// Get the positional arguments
	args := flag.Args()
	wd := args[1]
	box := args[0]
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd, "box: ", box)

	//	if *nsis_installer_mode {
	//		KillProcesses(wd)
	//	}

	time.Sleep(1 * time.Second)
	// 1. update files
	if err := LaunchInstaller(box, wd, *version, *chocolatey_source, *winget_install, *verbose, *name); err != nil {
		log.Printf("Update failed: %v", err)
		MessageBoxPlain("NekoBox Updater", "Update failed.\n\n"+err.Error())
		os.Exit(1)
	}
	// 2. start
	os.Chdir(wd)
	exec.Command("./nekobox.exe", args[2:]...).Start()
}

func LaunchInstaller(updatePackagePath string, installPath string, version string, chocolatey_source string, winget_install bool, verbose bool, name string) error {
	fmt.Printf("package %s install %s version %s name %s", updatePackagePath, installPath, version, name)

	if winget_install {
		winget_install = version != "" && updatePackagePath != "" && installPath != ""
	}
	if chocolatey_source != "" {
		if name == "" || version == "" {
			chocolatey_source = ""
		}
	}
	if winget_install {
		Launch("winget", "install", "--version", version, updatePackagePath, "--override", "/S /WINGET=1 /UNPACK=1 /D="+filepath.Clean(installPath))
	} else {
		command, err := newNSISCommand(updatePackagePath, installPath, chocolatey_source != "")
		if err != nil {
			return err
		}
		if chocolatey_source != "" {
			run_chocolatey(version, chocolatey_source, name)
		}
		return LaunchCmd(command)
	}
	return nil
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
		fmt.Println("<<<<Run Chocolatey Install>>>>")
		LaunchCmd(command)
	}
}
