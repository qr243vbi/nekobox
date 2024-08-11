package main

import (
	"flag"
	"log"
	"os"
	"os/exec"
	"path/filepath"
)

var local_qt_theme bool

func Launcher() {
	log.Println("Running as launcher")
	wd, _ := filepath.Abs(".")

	_debug := flag.Bool("debug", false, "Debug mode")
	flag.Parse()

	cmd := exec.Command("./nekoray", flag.Args()...)

	system_ld_env := os.Getenv("LD_LIBRARY_PATH")
	ld_env := "LD_LIBRARY_PATH="
	if len(system_ld_env) != 0 {
		ld_env += system_ld_env + ":"
	}
	ld_env += "/lib:/usr/lib:/lib64:/usr/lib/x86_64:/usr/local/Qt:/opt/Qt:"
	ld_env += filepath.Join(wd, "./usr/lib")

	system_qt_plugin_env := os.Getenv("QT_PLUGIN_PATH")
	qt_plugin_env := "QT_PLUGIN_PATH="
	if system_qt_plugin_env != "" {
		qt_plugin_env += system_qt_plugin_env + ":"
	}
	qt_plugin_env += "/usr/lib/qt6/plugins:/usr/lib/x86_64-linux-gnu/qt6/plugins:/usr/lib64/qt6/plugins:/usr/lib/qt/plugins:/usr/lib64/qt/plugins:"
	qt_plugin_env += filepath.Join(wd, "./usr/plugins")

	// Qt 5.12 abi is usually compatible with system Qt 5.15
	// But use package Qt 5.12 by default.
	cmd.Env = os.Environ()
	cmd.Env = append(cmd.Env, "NKR_FROM_LAUNCHER=1")
	cmd.Env = append(cmd.Env, ld_env, qt_plugin_env)
	log.Println(ld_env, qt_plugin_env, cmd)

	if *_debug {
		cmd.Env = append(cmd.Env, "QT_DEBUG_PLUGINS=1")
		cmd.Stdin = os.Stdin
		cmd.Stderr = os.Stderr
		cmd.Stdout = os.Stdout
		cmd.Run()
	} else {
		cmd.Start()
	}
}
