package main

import (
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"time"
	"runtime"
	"flag"
)

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}
	
	verbose := flag.Bool("verbose", false, "verbose mode")
	json_str := flag.String("options", "{}", "json options")

	// Parse the flags
	flag.Parse()
	
	run_json(*json_str);

	// Get the positional arguments
	args := flag.Args()

	wd := args[1]
	os.Chdir(wd)
	box := args[0]
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd, "box: ", box)
	{
		time.Sleep(2 * time.Second)
		{
			os.Chdir(wd)
			// 1. update files
			Updater(box, *verbose)
			// 2. start
			if runtime.GOOS == "windows" {
				exec.Command("./nekobox.exe",  args[2:]...).Start()
			} else {
				exec.Command("./nekobox",  args[2:]...).Start()
			}
		}
		return
	}
}

func Copy(src string, dst string) {
	srcFile, err := os.Open(src)
	if err != nil {
		log.Println(err)
		return
	}
	defer srcFile.Close()
	dstFile, err := os.OpenFile(dst, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 0644)
	if err != nil {
		log.Println(err)
		return
	}
	defer dstFile.Close()
	_, err = io.Copy(dstFile, srcFile)
	if err != nil {
		log.Println(err)
	}
}
