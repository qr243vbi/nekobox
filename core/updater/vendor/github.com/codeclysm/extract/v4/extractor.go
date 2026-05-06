package extract

import (
	"archive/tar"
	"archive/zip"
	"bytes"
	"compress/bzip2"
	"compress/gzip"
	"context"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	filetype "github.com/h2non/filetype"
	"github.com/h2non/filetype/types"
	"github.com/juju/errors"
	"github.com/klauspost/compress/zstd"
	"github.com/ulikunitz/xz"
)

// Extractor is more sophisticated than the base functions. It allows to write over an interface
// rather than directly on the filesystem
type Extractor struct {
	FS interface {
		// Link creates newname as a hard link to the oldname file. If there is an error, it will be of type *LinkError.
		Link(oldname, newname string) error

		// MkdirAll creates the directory path and all his parents if needed.
		MkdirAll(path string, perm os.FileMode) error

		// OpenFile opens the named file with specified flag (O_RDONLY etc.).
		OpenFile(name string, flag int, perm os.FileMode) (*os.File, error)

		// Symlink creates newname as a symbolic link to oldname.
		Symlink(oldname, newname string) error

		// Remove removes the named file or (empty) directory.
		Remove(path string) error

		// Stat returns a FileInfo describing the named file.
		Stat(name string) (os.FileInfo, error)

		// Chmod changes the mode of the named file to mode.
		// If the file is a symbolic link, it changes the mode of the link's target.
		Chmod(name string, mode os.FileMode) error
	}
}

// Archive extracts a generic archived stream of data in the specified location.
// It automatically detects the archive type and accepts a rename function to
// handle the names of the files.
// If the file is not an archive, an error is returned.
func (e *Extractor) Archive(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	body, kind, err := match(body)
	if err != nil {
		errors.Annotatef(err, "Detect archive type")
	}

	switch kind.Extension {
	case "zip":
		return e.Zip(ctx, body, location, rename)
	case "gz":
		return e.Gz(ctx, body, location, rename)
	case "bz2":
		return e.Bz2(ctx, body, location, rename)
	case "xz":
		return e.Xz(ctx, body, location, rename)
	case "zst":
		return e.Zstd(ctx, body, location, rename)
	case "tar":
		return e.Tar(ctx, body, location, rename)
	default:
		return errors.New("Not a supported archive: " + kind.Extension)
	}
}

func (e *Extractor) Zstd(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	reader, err := zstd.NewReader(body)
	if err != nil {
		return errors.Annotatef(err, "opening zstd: detect")
	}

	body, kind, err := match(reader)
	if err != nil {
		return errors.Annotatef(err, "extract zstd: detect")
	}

	if kind.Extension == "tar" {
		return e.Tar(ctx, body, location, rename)
	}

	err = e.copy(ctx, location, 0666, body)
	if err != nil {
		return err
	}
	return nil
}

func (e *Extractor) Xz(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	reader, err := xz.NewReader(body)
	if err != nil {
		return errors.Annotatef(err, "opening xz: detect")
	}

	body, kind, err := match(reader)
	if err != nil {
		return errors.Annotatef(err, "extract xz: detect")
	}

	if kind.Extension == "tar" {
		return e.Tar(ctx, body, location, rename)
	}

	err = e.copy(ctx, location, 0666, body)
	if err != nil {
		return err
	}
	return nil
}

// Bz2 extracts a .bz2 or .tar.bz2 archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func (e *Extractor) Bz2(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	reader := bzip2.NewReader(body)

	body, kind, err := match(reader)
	if err != nil {
		return errors.Annotatef(err, "extract bz2: detect")
	}

	if kind.Extension == "tar" {
		return e.Tar(ctx, body, location, rename)
	}

	err = e.copy(ctx, location, 0666, body)
	if err != nil {
		return err
	}
	return nil
}

// Gz extracts a .gz or .tar.gz archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func (e *Extractor) Gz(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	reader, err := gzip.NewReader(body)
	if err != nil {
		return errors.Annotatef(err, "Gunzip")
	}

	body, kind, err := match(reader)
	if err != nil {
		return err
	}

	if kind.Extension == "tar" {
		return e.Tar(ctx, body, location, rename)
	}
	err = e.copy(ctx, location, 0666, body)
	if err != nil {
		return err
	}
	return nil
}

type link struct {
	Name string
	Path string
}

// Tar extracts a .tar archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func (e *Extractor) Tar(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	links := []*link{}
	symlinks := []*link{}

	// We make the first pass creating the directory structure, or we could end up
	// attempting to create a file where there's no folder
	tr := tar.NewReader(body)
	for {
		select {
		case <-ctx.Done():
			return errors.New("interrupted")
		default:
		}

		header, err := tr.Next()
		if err == io.EOF {
			break
		}

		if err != nil {
			return errors.Annotatef(err, "Read tar stream")
		}

		path := header.Name
		if rename != nil {
			path = rename(path)
		}

		if path == "" {
			continue
		}

		if path, err = safeJoin(location, path); err != nil {
			continue
		}

		info := header.FileInfo()

		switch header.Typeflag {
		case tar.TypeDir:
			if err := e.FS.MkdirAll(path, info.Mode()); err != nil {
				return errors.Annotatef(err, "Create directory %s", path)
			}
		case tar.TypeReg, tar.TypeRegA:
			if err := e.copy(ctx, path, info.Mode(), tr); err != nil {
				return errors.Annotatef(err, "Create file %s", path)
			}
		case tar.TypeLink:
			name := header.Linkname
			if rename != nil {
				name = rename(name)
			}

			name, err = safeJoin(location, name)
			if err != nil {
				continue
			}
			links = append(links, &link{Path: path, Name: name})
		case tar.TypeSymlink:
			symlinks = append(symlinks, &link{Path: path, Name: header.Linkname})
		}
	}

	// Now we make another pass creating the links
	for i := range links {
		select {
		case <-ctx.Done():
			return errors.New("interrupted")
		default:
		}
		_ = e.FS.Remove(links[i].Path)
		if err := e.FS.Link(links[i].Name, links[i].Path); err != nil {
			return errors.Annotatef(err, "Create link %s", links[i].Path)
		}
	}

	if err := e.extractSymlinks(ctx, symlinks); err != nil {
		return err
	}

	return nil
}

func (e *Extractor) extractSymlinks(ctx context.Context, symlinks []*link) error {
	for _, symlink := range symlinks {
		select {
		case <-ctx.Done():
			return errors.New("interrupted")
		default:
		}

		// Make a placeholder and replace it after unpacking everything
		_ = e.FS.Remove(symlink.Path)
		f, err := e.FS.OpenFile(symlink.Path, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, os.FileMode(0666))
		if err != nil {
			return fmt.Errorf("creating symlink placeholder %s: %w", symlink.Path, err)
		}
		if err := f.Close(); err != nil {
			return fmt.Errorf("creating symlink placeholder %s: %w", symlink.Path, err)
		}
	}

	for _, symlink := range symlinks {
		select {
		case <-ctx.Done():
			return errors.New("interrupted")
		default:
		}
		_ = e.FS.Remove(symlink.Path)
		if err := e.FS.Symlink(symlink.Name, symlink.Path); err != nil {
			return errors.Annotatef(err, "Create link %s", symlink.Path)
		}
	}

	return nil
}

// Zip extracts a .zip archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example).
func (e *Extractor) Zip(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	var bodySize int64
	bodyReaderAt, isReaderAt := (body).(io.ReaderAt)
	if bodySeeker, isSeeker := (body).(io.Seeker); isReaderAt && isSeeker {
		// get the size by seeking to the end
		endPos, err := bodySeeker.Seek(0, io.SeekEnd)
		if err != nil {
			return fmt.Errorf("failed to seek to the end of the body: %s", err)
		}
		// reset the reader to the beginning
		if _, err := bodySeeker.Seek(0, io.SeekStart); err != nil {
			return fmt.Errorf("failed to seek to the beginning of the body: %w", err)
		}
		bodySize = endPos
	} else {
		// read the whole body into a buffer. Not sure this is the best way to do it
		buffer := bytes.NewBuffer([]byte{})
		copyCancel(ctx, buffer, body)
		bodyReaderAt = bytes.NewReader(buffer.Bytes())
		bodySize = int64(buffer.Len())
	}
	archive, err := zip.NewReader(bodyReaderAt, bodySize)
	if err != nil {
		return errors.Annotatef(err, "Read the zip file")
	}

	links := []*link{}

	// We make the first pass creating the directory structure, or we could end up
	// attempting to create a file where there's no folder
	for _, header := range archive.File {
		select {
		case <-ctx.Done():
			return errors.New("interrupted")
		default:
		}

		path := header.Name

		// Replace backslash with forward slash. There are archives in the wild made with
		// buggy compressors that use backslash as path separator. The ZIP format explicitly
		// denies the use of "\" so we just replace it with slash "/".
		// Moreover it seems that folders are stored as "files" but with a final "\" in the
		// filename... oh, well...
		forceDir := strings.HasSuffix(path, "\\")
		path = strings.Replace(path, "\\", "/", -1)

		if rename != nil {
			path = rename(path)
		}

		if path == "" {
			continue
		}

		if path, err = safeJoin(location, path); err != nil {
			continue
		}

		info := header.FileInfo()

		switch {
		case info.IsDir() || forceDir:
			dirMode := info.Mode() | os.ModeDir | 0100
			if _, err := e.FS.Stat(path); err == nil {
				// directory already created, update permissions
				if err := e.FS.Chmod(path, dirMode); err != nil {
					return errors.Annotatef(err, "Set permissions %s", path)
				}
			} else if err := e.FS.MkdirAll(path, dirMode); err != nil {
				return errors.Annotatef(err, "Create directory %s", path)
			}
		// We only check for symlinks because hard links aren't possible
		case info.Mode()&os.ModeSymlink != 0:
			if f, err := header.Open(); err != nil {
				return errors.Annotatef(err, "Open link %s", path)
			} else if name, err := io.ReadAll(f); err != nil {
				return errors.Annotatef(err, "Read address of link %s", path)
			} else {
				links = append(links, &link{Path: path, Name: string(name)})
				f.Close()
			}
		default:
			if f, err := header.Open(); err != nil {
				return errors.Annotatef(err, "Open file %s", path)
			} else if err := e.copy(ctx, path, info.Mode(), f); err != nil {
				return errors.Annotatef(err, "Create file %s", path)
			} else {
				f.Close()
			}
		}
	}

	if err := e.extractSymlinks(ctx, links); err != nil {
		return err
	}

	return nil
}

func (e *Extractor) copy(ctx context.Context, path string, mode os.FileMode, src io.Reader) error {
	// We add the execution permission to be able to create files inside it
	err := e.FS.MkdirAll(filepath.Dir(path), mode|os.ModeDir|0100)
	if err != nil {
		return err
	}
	_ = e.FS.Remove(path)
	file, err := e.FS.OpenFile(path, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if err != nil {
		return err
	}
	defer file.Close()
	_, err = copyCancel(ctx, file, src)
	return err
}

// match reads the first 512 bytes, calls types.Match and returns a reader
// for the whole stream
func match(r io.Reader) (io.Reader, types.Type, error) {
	buffer := make([]byte, 512)

	n, err := r.Read(buffer)
	if err != nil && err != io.EOF {
		return nil, types.Unknown, err
	}

	if seeker, ok := r.(io.Seeker); ok {
		// if the stream is seekable, we just rewind it
		if _, err := seeker.Seek(0, io.SeekStart); err != nil {
			return nil, types.Unknown, err
		}
	} else {
		// otherwise we create a new reader that will prepend the buffer
		r = io.MultiReader(bytes.NewBuffer(buffer[:n]), r)
	}

	typ, err := filetype.Match(buffer)

	return r, typ, err
}

// safeJoin performs a filepath.Join of 'parent' and 'subdir' but returns an error
// if the resulting path points outside of 'parent'.
func safeJoin(parent, subdir string) (string, error) {
	res := filepath.Join(parent, subdir)
	if !strings.HasSuffix(parent, string(os.PathSeparator)) {
		parent += string(os.PathSeparator)
	}
	if !strings.HasPrefix(res, parent) {
		return res, errors.Errorf("unsafe path join: '%s' with '%s'", parent, subdir)
	}
	return res, nil
}
