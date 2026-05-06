package rw

import (
	"encoding/json"
	"io"
	"os"
	"strings"

	"github.com/sagernet/sing/common"
)

func IsFile(path string) bool {
	stat, err := os.Stat(path)
	if err != nil {
		return false
	}
	return !stat.IsDir()
}

func IsDir(path string) bool {
	stat, err := os.Stat(path)
	if err != nil {
		return false
	}
	return stat.IsDir()
}

func MkdirParent(path string) error {
	if strings.Contains(path, string(os.PathSeparator)) {
		parent := path[:strings.LastIndex(path, string(os.PathSeparator))]
		if !IsDir(parent) {
			err := os.MkdirAll(parent, 0o755)
			if err != nil {
				return err
			}
		}
	}
	return nil
}

func CopyFile(srcPath, dstPath string) error {
	srcFile, err := os.Open(srcPath)
	if err != nil {
		return err
	}
	defer srcFile.Close()
	srcStat, err := srcFile.Stat()
	if err != nil {
		return err
	}
	err = MkdirParent(dstPath)
	if err != nil {
		return err
	}
	dstFile, err := os.OpenFile(dstPath, os.O_RDWR|os.O_CREATE|os.O_TRUNC, srcStat.Mode())
	if err != nil {
		return err
	}
	defer dstFile.Close()
	_, err = io.Copy(dstFile, srcFile)
	return err
}

// Deprecated: use IsFile and IsDir instead.
func FileExists(path string) bool {
	return common.Error(os.Stat(path)) == nil
}

// Deprecated: use MkdirParent and os.WriteFile instead.
func WriteFile(path string, content []byte) error {
	if strings.Contains(path, "/") {
		parent := path[:strings.LastIndex(path, "/")]
		if !FileExists(parent) {
			err := os.MkdirAll(parent, 0o755)
			if err != nil {
				return err
			}
		}
	}

	file, err := os.Create(path)
	if err != nil {
		return err
	}
	defer file.Close()
	_, err = file.Write(content)
	return err
}

// Deprecated: wtf is this?
func ReadJSON(path string, data any) error {
	content, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	err = json.Unmarshal(content, data)
	if err != nil {
		return err
	}
	return nil
}

// Deprecated: wtf is this?
func WriteJSON(path string, data any) error {
	content, err := json.Marshal(data)
	if err != nil {
		return err
	}
	return WriteFile(path, content)
}
