package main

import (
	"fmt"
	"strings"
	"testing"
)

// Model the /D recognition in NSIS Source/exehead/Main.c, NSISWinMainNOCRT.
// /D must be the last switch, preceded by a space rather than a quote:
// https://nsis.sourceforge.io/Docs/Chapter3.html#installerusage
// https://github.com/kichik/nsis/blob/master/Source/exehead/Main.c
func nsisDestination(command string) string {
	pos, delimiter := 0, byte(' ')
	if strings.HasPrefix(command, `"`) {
		pos++
		delimiter = '"'
	}
	next := strings.IndexByte(command[pos:], delimiter)
	if next < 0 {
		return ""
	}
	pos += next + 1
	for pos < len(command) {
		for pos < len(command) && command[pos] == ' ' {
			pos++
		}
		if pos == len(command) {
			break
		}
		delimiter = ' '
		if command[pos] == '"' {
			pos++
			delimiter = '"'
		}
		if pos < len(command) && command[pos] == '/' {
			pos++
			if pos >= 2 && pos+2 <= len(command) && command[pos-2:pos+2] == " /D=" {
				return command[pos+2:]
			}
		}
		next = strings.IndexByte(command[pos:], delimiter)
		if next < 0 {
			break
		}
		pos += next
		if pos < len(command) && command[pos] == '"' {
			pos++
		}
	}
	return ""
}

func TestNSISInstallArgumentsPreserveDestination(t *testing.T) {
	for _, destination := range []string{
		`D:\NekoBox`, `D:\Portable Apps\NekoBox`, `C:\Program Files\NekoBox`,
		`D:\便携 应用\NekoBox`, `\\server\Shared Apps\NekoBox`, `D:\`,
		`D:\Apps & Tools (64-bit)\NekoBox`,
	} {
		for _, chocolatey := range []bool{false, true} {
			t.Run(fmt.Sprintf("%s/chocolatey=%t", destination, chocolatey), func(t *testing.T) {
				args, err := nsisInstallArguments(destination, chocolatey)
				if err != nil {
					t.Fatal(err)
				}
				flag := "0"
				if chocolatey {
					flag = "1"
				}
				want := "/S /UNPACK=1 /CHOCOLATEY=" + flag + " /D=" + destination
				if args != want {
					t.Fatalf("arguments = %q, want %q", args, want)
				}
				if got := nsisDestination(`"C:\Downloaded Updates\installer.exe" ` + args); got != destination {
					t.Fatalf("NSIS destination = %q, want %q", got, destination)
				}
			})
		}
	}
}

func TestNSISInstallArgumentsRejectUnsafePaths(t *testing.T) {
	for _, path := range []string{"", `D:\"quoted"\NekoBox`, "D:\\Apps\x00\\NekoBox", "D:\\Apps\r\\NekoBox", "D:\\Apps\n\\NekoBox", "D:\\Apps\t\\NekoBox", "D:\\Apps\x7f\\NekoBox"} {
		if args, err := nsisInstallArguments(path, false); err == nil || args != "" {
			t.Errorf("unsafe path %q produced arguments %q, error %v", path, args, err)
		}
	}
}

func TestNSISParserRejectsOrdinaryQuotedDirectoryArgument(t *testing.T) {
	const command = `"C:\Downloads\installer.exe" /S /UNPACK=1 "/D=D:\Portable Apps\NekoBox"`
	if got := nsisDestination(command); got != "" {
		t.Fatalf("quoted /D unexpectedly recognized: %q", got)
	}
}
