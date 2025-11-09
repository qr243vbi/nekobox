package main

import (
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
)

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}

	wd := os.Args[2]
	os.Chdir(wd)
	box := os.Args[1]
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd, "box: ", box)
	{
		{
			os.Chdir(wd)
			// 1. update files
			Updater(box)
			// 2. start
			if runtime.GOOS == "windows" {
				exec.Command("./nekobox.exe",  os.Args[3:]...).Start()
			} else {
				exec.Command("./nekobox",  os.Args[3:]...).Start()
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
