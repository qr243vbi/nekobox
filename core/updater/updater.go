package main

import (
	"context"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/codeclysm/extract/v4"
)

func Updater() {
	pre_cleanup := func() {
		if runtime.GOOS == "linux" {
			os.RemoveAll("./usr")
		}
		os.RemoveAll("./nekobox_update")
	}

	// find update package
	var updatePackagePath string
	if len(os.Args) == 2 && Exist(os.Args[1]) {
		updatePackagePath = os.Args[1]
	} else if Exist("./nekobox.zip") {
		updatePackagePath = "./nekobox.zip"
	} else if Exist("./nekobox.tar.gz") {
		updatePackagePath = "./nekobox.tar.gz"
	} else {
		log.Fatalln("no update")
	}
	log.Println("updating from", updatePackagePath)

	dir, err := os.Getwd()
	if err != nil {
		log.Fatalln(err)
	}
	// extract update package
	if strings.HasSuffix(updatePackagePath, ".zip") {
		pre_cleanup()
		f, err := os.Open(updatePackagePath)
		if err != nil {
			log.Fatalln(err.Error())
		}
		err = extract.Zip(context.Background(), f, dir+string(os.PathSeparator)+"nekobox_update", nil)
		if err != nil {
			log.Fatalln(err.Error())
		}
		f.Close()
	} else if strings.HasSuffix(updatePackagePath, ".tar.gz") {
		pre_cleanup()
		f, err := os.Open(updatePackagePath)
		if err != nil {
			log.Fatalln(err.Error())
		}
		err = extract.Gz(context.Background(), f, dir+string(os.PathSeparator)+"nekobox_update", nil)
		if err != nil {
			log.Fatalln(err.Error())
		}
		f.Close()
	}

	// remove old file
	removeAll("./*.dll")	
	removeAll("./*.dmp")

	// os.RemoveAll("./run_admin.ps1")
	os.RemoveAll("./check_new_release.js")

	// update move
	err = Mv("./nekobox_update/nekobox", "./")
	if err != nil {
		MessageBoxPlain("nekobox Updater", "Update failed. Please close the running instance and run the updater again.\n\n"+err.Error())
		log.Fatalln(err.Error())
	}

	os.RemoveAll("./nekobox_update")
	os.RemoveAll("./nekobox.zip")
	os.RemoveAll("./nekobox.tar.gz")
}

func Exist(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func FindExist(paths []string) string {
	for _, path := range paths {
		if Exist(path) {
			return path
		}
	}
	return ""
}

func Mv(src, dst string) error {
	s, err := os.Stat(src)
	if err != nil {
		return err
	}
	if s.IsDir() {
		es, err := os.ReadDir(src)
		if err != nil {
			return err
		}
		for _, e := range es {
			err = Mv(filepath.Join(src, e.Name()), filepath.Join(dst, e.Name()))
			if err != nil {
				return err
			}
		}
	} else {
		err = os.MkdirAll(filepath.Dir(dst), 0755)
		if err != nil {
			return err
		}
		err = os.Rename(src, dst)
		if err != nil {
			return err
		}
	}
	return nil
}

func removeAll(glob string) {
	files, _ := filepath.Glob(glob)
	for _, f := range files {
		os.Remove(f)
	}
}
