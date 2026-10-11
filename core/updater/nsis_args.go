package main

import (
	"fmt"
	"strings"
	"unicode"
)

func validateNSISPath(path string) error {
	if path == "" {
		return fmt.Errorf("path is empty")
	}
	if strings.ContainsRune(path, '"') || strings.ContainsFunc(path, unicode.IsControl) {
		return fmt.Errorf("path contains a quote or control character")
	}
	return nil
}

// The caller supplies a cleaned, absolute Windows path. NSIS requires /D to
// remain unquoted and last, even when the destination contains spaces.
// https://nsis.sourceforge.io/Docs/Chapter3.html#installerusage
func nsisInstallArguments(installPath string, chocolatey bool) (string, error) {
	if err := validateNSISPath(installPath); err != nil {
		return "", err
	}
	chocolateyFlag := "0"
	if chocolatey {
		chocolateyFlag = "1"
	}
	return "/S /UNPACK=1 /CHOCOLATEY=" + chocolateyFlag + " /D=" + installPath, nil
}
