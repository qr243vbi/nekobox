// Package extract allows to extract archives in zip, tar.gz or tar.bz2 formats
// easily.
//
// Most of the time you'll just need to call the proper function with a Reader and
// a destination:
//
//	file, _ := os.Open("path/to/file.tar.bz2")
//	extract.Bz2(context.TODO, file, "/path/where/to/extract", nil)
//
// ```
//
// Sometimes you'll want a bit more control over the files, such as extracting a
// subfolder of the archive. In this cases you can specify a renamer func that will
// change the path for every file:
//
//	var shift = func(path string) string {
//		parts := strings.Split(path, string(filepath.Separator))
//		parts = parts[1:]
//		return strings.Join(parts, string(filepath.Separator))
//	}
//	extract.Bz2(context.TODO, file, "/path/where/to/extract", shift)
//
// ```
//
// If you don't know which archive you're dealing with (life really is always a surprise) you can use Archive, which will infer the type of archive from the first bytes
//
//	extract.Archive(context.TODO, file, "/path/where/to/extract", nil)
package extract

import (
	"context"
	"io"
	"os"
)

// Renamer is a function that can be used to rename the files when you're extracting
// them. For example you may want to only extract files with a certain pattern.
// If you return an empty string they won't be extracted.
type Renamer func(string) string

// Archive extracts a generic archived stream of data in the specified location.
// It automatically detects the archive type and accepts a rename function to
// handle the names of the files.
// If the file is not an archive, an error is returned.
func Archive(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Archive(ctx, body, location, rename)
}

// Zstd extracts a .zst or .tar.zst archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func Zstd(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Zstd(ctx, body, location, rename)
}

// Xz extracts a .xz or .tar.xz archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func Xz(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Xz(ctx, body, location, rename)
}

// Bz2 extracts a .bz2 or .tar.bz2 archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func Bz2(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Bz2(ctx, body, location, rename)
}

// Gz extracts a .gz or .tar.gz archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func Gz(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Gz(ctx, body, location, rename)
}

// Tar extracts a .tar archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example)
func Tar(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Tar(ctx, body, location, rename)
}

// Zip extracts a .zip archived stream of data in the specified location.
// It accepts a rename function to handle the names of the files (see the example).
func Zip(ctx context.Context, body io.Reader, location string, rename Renamer) error {
	extractor := Extractor{FS: fs{}}
	return extractor.Zip(ctx, body, location, rename)
}

type fs struct{}

func (f fs) Link(oldname, newname string) error {
	return os.Link(oldname, newname)
}

func (f fs) MkdirAll(path string, perm os.FileMode) error {
	return os.MkdirAll(path, perm)
}

func (f fs) Symlink(oldname, newname string) error {
	return os.Symlink(oldname, newname)
}

func (f fs) OpenFile(name string, flag int, perm os.FileMode) (*os.File, error) {
	return os.OpenFile(name, flag, perm)
}

func (f fs) Remove(path string) error {
	return os.Remove(path)
}

func (f fs) Stat(name string) (os.FileInfo, error) {
	return os.Stat(name)
}

func (f fs) Chmod(name string, mode os.FileMode) error {
	return os.Chmod(name, mode)
}
