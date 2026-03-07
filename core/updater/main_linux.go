package main

import (
	"flag"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"time"
)

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}
	verbose := flag.Bool("verbose", false, "verbose mode")
	// Parse the flags
	flag.Parse()
	// Get the positional arguments
	args := flag.Args()
	wd := args[1]
	box := args[0]
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd, "box: ", box)
	time.Sleep(2 * time.Second)
	os.Chdir(wd)
	Updater(box, *verbose)
	os.Chmod("nekobox", 0755)
	os.Chmod("nekobox_core", 0755)
	os.Chmod("updater", 0755)
	exec.Command("./nekobox", args[2:]...).Start()
}
