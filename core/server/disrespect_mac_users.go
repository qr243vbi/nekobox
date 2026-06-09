//go:build darwin
package main

import (
	"fmt"
	"os"
)

func main(){
	fmt.Println("Sending hello to the dumbass who bought macOS")
	os.Exit(1)
}
