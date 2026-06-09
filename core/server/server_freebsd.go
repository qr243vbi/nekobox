//go:build freebsd
package main

import (
	"os"
)

func checkFlags(save bool){
	
}


func isElevated() (bool, error) {
	if os.Geteuid() == 0 {
		return true, nil
	}
	return false, nil
}


func CheckResolvectl() {
}



func RunResolvectl(args ...string) error {
	return nil
}
