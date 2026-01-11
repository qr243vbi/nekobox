package filemanager

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/sagernet/sing/common/rw"
	"github.com/sagernet/sing/service"
)

var _ Manager = (*defaultManager)(nil)

type defaultManager struct {
	basePath string
	tempPath string
	chown    bool
	userID   int
	groupID  int
}

func WithDefault(ctx context.Context, basePath string, tempPath string, userID int, groupID int) context.Context {
	chown := userID != os.Getuid() || groupID != os.Getgid()
	if tempPath == "" {
		tempPath = os.TempDir()
	}
	return service.ContextWith[Manager](ctx, &defaultManager{
		basePath: basePath,
		tempPath: tempPath,
		chown:    chown,
		userID:   userID,
		groupID:  groupID,
	})
}

func (m *defaultManager) BasePath(name string) string {
	if m.basePath == "" || strings.HasPrefix(name, "/") {
		return name
	}
	return filepath.Join(m.basePath, name)
}

func (m *defaultManager) OpenFile(name string, flag int, perm os.FileMode) (*os.File, error) {
	name = m.BasePath(name)
	willCreate := flag&os.O_CREATE != 0 && !rw.IsFile(name)
	file, err := os.OpenFile(name, flag, perm)
	if err != nil {
		return nil, err
	}
	if m.chown && willCreate {
		err = file.Chown(m.userID, m.groupID)
		if err != nil {
			file.Close()
			os.Remove(file.Name())
			return nil, err
		}
	}
	return file, nil
}

func (m *defaultManager) Create(name string) (*os.File, error) {
	name = m.BasePath(name)
	file, err := os.Create(name)
	if err != nil {
		return nil, err
	}
	if m.chown {
		err = file.Chown(m.userID, m.groupID)
		if err != nil {
			file.Close()
			os.Remove(file.Name())
			return nil, err
		}
	}
	return file, nil
}

func (m *defaultManager) CreateTemp(pattern string) (*os.File, error) {
	file, err := os.CreateTemp(m.tempPath, pattern)
	if err != nil {
		return nil, err
	}
	if m.chown {
		err = file.Chown(m.userID, m.groupID)
		if err != nil {
			file.Close()
			os.Remove(file.Name())
			return nil, err
		}
	}
	return file, nil
}

func (m *defaultManager) Chown(path string) error {
	if m.chown {
		return os.Chown(path, m.userID, m.groupID)
	}
	return nil
}

func (m *defaultManager) Mkdir(path string, perm os.FileMode) error {
	path = m.BasePath(path)
	err := os.Mkdir(path, perm)
	if err != nil {
		return err
	}
	if m.chown {
		err = os.Chown(path, m.userID, m.groupID)
		if err != nil {
			os.Remove(path)
			return err
		}
	}
	return nil
}

func (m *defaultManager) MkdirAll(path string, perm os.FileMode) error {
	path = m.BasePath(path)
	return m.mkdirAll(path, perm)
}

func (m *defaultManager) mkdirAll(path string, perm os.FileMode) error {
	dir, err := os.Stat(path)
	if err == nil {
		if dir.IsDir() {
			return nil
		}
		return &os.PathError{Op: "mkdir", Path: path, Err: syscall.ENOTDIR}
	}

	i := len(path)
	for i > 0 && os.IsPathSeparator(path[i-1]) {
		i--
	}

	j := i
	for j > 0 && !os.IsPathSeparator(path[j-1]) {
		j--
	}

	if j > 1 {
		err = m.MkdirAll(fixRootDirectory(path[:j-1]), perm)
		if err != nil {
			return err
		}
	}

	err = os.Mkdir(path, perm)
	if err != nil {
		dir, err1 := os.Lstat(path)
		if err1 == nil && dir.IsDir() {
			return nil
		}
		return err
	}
	if m.chown {
		err = os.Chown(path, m.userID, m.groupID)
		if err != nil {
			os.Remove(path)
			return err
		}
	}
	return nil
}

func (m *defaultManager) Remove(path string) error {
	path = m.BasePath(path)
	return os.Remove(path)
}

func (m *defaultManager) RemoveAll(path string) error {
	path = m.BasePath(path)
	return os.RemoveAll(path)
}

func fixRootDirectory(p string) string {
	if len(p) == len(`\\?\c:`) {
		if os.IsPathSeparator(p[0]) && os.IsPathSeparator(p[1]) && p[2] == '?' && os.IsPathSeparator(p[3]) && p[5] == ':' {
			return p + `\`
		}
	}
	return p
}
