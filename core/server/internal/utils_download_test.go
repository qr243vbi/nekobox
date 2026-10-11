package internal

import (
	"errors"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

type downloadRoundTripper func(*http.Request) (*http.Response, error)

func (f downloadRoundTripper) RoundTrip(req *http.Request) (*http.Response, error) {
	return f(req)
}

func useDownloadTransport(t *testing.T, f downloadRoundTripper) {
	t.Helper()
	previous := http.DefaultTransport
	http.DefaultTransport = f
	t.Cleanup(func() { http.DefaultTransport = previous })
}

func downloadResponse(status int, body io.ReadCloser) *http.Response {
	return &http.Response{StatusCode: status, Status: http.StatusText(status), Body: body}
}

type failingDownloadBody struct{ read bool }

func (b *failingDownloadBody) Read(p []byte) (int, error) {
	if b.read {
		return 0, io.ErrUnexpectedEOF
	}
	b.read = true
	return copy(p, "incomplete"), io.ErrUnexpectedEOF
}

func (*failingDownloadBody) Close() error { return nil }

func assertDownloadContents(t *testing.T, path, want string) {
	t.Helper()
	got, err := os.ReadFile(path)
	if err != nil || string(got) != want {
		t.Fatalf("destination = %q, %v; want %q", got, err, want)
	}
}

func assertDownloadDirectory(t *testing.T, dir string, want ...string) {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}
	if len(entries) != len(want) {
		t.Fatalf("directory contains %v; want only %v", entries, want)
	}
	for i, name := range want {
		if entries[i].Name() != name {
			t.Fatalf("directory entry = %q; want %q", entries[i].Name(), name)
		}
	}
}

func TestDownloadFileFailurePreservesDestination(t *testing.T) {
	cases := []struct {
		name      string
		url       string
		transport downloadRoundTripper
	}{
		{"invalid URL", ":bad", func(*http.Request) (*http.Response, error) { t.Fatal("unexpected request"); return nil, nil }},
		{"invalid request", "https://cache.invalid/\x7f", func(*http.Request) (*http.Response, error) { t.Fatal("unexpected request"); return nil, nil }},
		{"transport failure", "https://cache.invalid/rules.srs", func(*http.Request) (*http.Response, error) { return nil, errors.New("offline") }},
		{"HTTP failure", "https://cache.invalid/rules.srs", func(*http.Request) (*http.Response, error) {
			return downloadResponse(503, io.NopCloser(strings.NewReader("unavailable"))), nil
		}},
		{"truncated body", "https://cache.invalid/rules.srs", func(*http.Request) (*http.Response, error) { return downloadResponse(200, &failingDownloadBody{}), nil }},
	}
	for _, tc := range cases {
		for _, existing := range []bool{false, true} {
			t.Run(tc.name+map[bool]string{false: "/new", true: "/existing"}[existing], func(t *testing.T) {
				dir := t.TempDir()
				path := filepath.Join(dir, "rules.srs")
				if existing {
					if err := os.WriteFile(path, []byte("known good"), 0600); err != nil {
						t.Fatal(err)
					}
				}
				useDownloadTransport(t, tc.transport)
				if err := DownloadFile(tc.url, path, true); err == nil {
					t.Fatal("expected download failure")
				}
				if existing {
					assertDownloadContents(t, path, "known good")
					assertDownloadDirectory(t, dir, "rules.srs")
				} else {
					if _, err := os.Stat(path); !os.IsNotExist(err) {
						t.Fatalf("failed download published destination: %v", err)
					}
					assertDownloadDirectory(t, dir)
				}
			})
		}
	}
}

func TestDownloadFileSuccessReplacesDestination(t *testing.T) {
	for _, existing := range []bool{false, true} {
		t.Run(map[bool]string{false: "new", true: "existing"}[existing], func(t *testing.T) {
			dir := filepath.Join(t.TempDir(), "nested")
			path := filepath.Join(dir, "rules.srs")
			if existing {
				if err := os.MkdirAll(dir, 0755); err != nil {
					t.Fatal(err)
				}
				if err := os.WriteFile(path, []byte("old longer payload"), 0600); err != nil {
					t.Fatal(err)
				}
			}
			useDownloadTransport(t, func(req *http.Request) (*http.Response, error) {
				if req.Method != "GET" {
					t.Errorf("method = %s", req.Method)
				}
				if req.URL.User != nil {
					t.Error("credentials retained in request URL")
				}
				user, password, ok := req.BasicAuth()
				if !ok || user != "user" || password != "pass" {
					t.Error("basic authentication changed")
				}
				return downloadResponse(200, io.NopCloser(strings.NewReader("complete"))), nil
			})
			if err := DownloadFile("https://user:pass@cache.invalid/rules.srs", path, true); err != nil {
				t.Fatal(err)
			}
			assertDownloadContents(t, path, "complete")
			assertDownloadDirectory(t, dir, "rules.srs")
		})
	}
}

func TestDownloadFilePreservesCreationPermissions(t *testing.T) {
	if runtime.GOOS == "windows" {
		t.Skip("Unix permission bits")
	}
	for _, existingMode := range []os.FileMode{0, 0600, 0640} {
		t.Run(existingMode.String(), func(t *testing.T) {
			dir := t.TempDir()
			path := filepath.Join(dir, "rules.srs")
			want := existingMode
			if existingMode != 0 {
				if err := os.WriteFile(path, []byte("known good"), existingMode); err != nil {
					t.Fatal(err)
				}
				if err := os.Chmod(path, existingMode); err != nil {
					t.Fatal(err)
				}
			} else {
				// Compare with os.Create rather than assuming a particular umask.
				control, err := os.Create(filepath.Join(dir, "control"))
				if err != nil {
					t.Fatal(err)
				}
				info, err := control.Stat()
				if err != nil {
					t.Fatal(err)
				}
				want = info.Mode().Perm()
				if err := control.Close(); err != nil {
					t.Fatal(err)
				}
			}
			useDownloadTransport(t, func(*http.Request) (*http.Response, error) {
				return downloadResponse(200, io.NopCloser(strings.NewReader("complete"))), nil
			})
			if err := DownloadFile("https://cache.invalid/rules.srs", path, true); err != nil {
				t.Fatal(err)
			}
			info, err := os.Stat(path)
			if err != nil {
				t.Fatal(err)
			}
			if got := info.Mode().Perm(); got != want {
				t.Fatalf("permission mode = %04o; want %04o", got, want)
			}
		})
	}
}

func TestDownloadFileRenameFailureCleansTemporaryFile(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "rules.srs")
	if err := os.Mkdir(path, 0755); err != nil {
		t.Fatal(err)
	}
	sentinel := filepath.Join(path, "keep")
	if err := os.WriteFile(sentinel, []byte("keep"), 0600); err != nil {
		t.Fatal(err)
	}
	requested := false
	useDownloadTransport(t, func(*http.Request) (*http.Response, error) {
		requested = true
		return downloadResponse(200, io.NopCloser(strings.NewReader("complete"))), nil
	})
	if err := DownloadFile("https://cache.invalid/rules.srs", path, true); err == nil {
		t.Fatal("expected rename failure")
	}
	if !requested {
		t.Error("failure occurred before downloading the replacement")
	}
	assertDownloadContents(t, sentinel, "keep")
	assertDownloadDirectory(t, dir, "rules.srs")
}

func TestCacheHttpBoolRetriesAfterFailure(t *testing.T) {
	previous := ruleset_cachedir
	ruleset_cachedir = t.TempDir()
	t.Cleanup(func() { ruleset_cachedir = previous })
	requests := 0
	useDownloadTransport(t, func(*http.Request) (*http.Response, error) {
		requests++
		if requests == 1 {
			return downloadResponse(200, &failingDownloadBody{}), nil
		}
		return downloadResponse(200, io.NopCloser(strings.NewReader("complete"))), nil
	})
	url := "https://cache.invalid/rules.srs"
	downloaded := true
	if got := CacheHttpBool(url, true, &downloaded); got != "" || downloaded {
		t.Fatalf("failed download = %q, %v", got, downloaded)
	}
	got := CacheHttpBool(url, true, &downloaded)
	if got != urlToPath(url) || !downloaded || requests != 2 {
		t.Fatalf("retry = %q, %v, %d requests", got, downloaded, requests)
	}
	assertDownloadContents(t, got, "complete")
	if hit := CacheHttpBool(url, true, &downloaded); hit != got || downloaded || requests != 2 {
		t.Fatalf("cache hit = %q, %v, %d requests", hit, downloaded, requests)
	}
	assertDownloadDirectory(t, filepath.Dir(got), "rules.srs")
}

type blockedDownloadBody struct {
	started chan struct{}
	resume  chan struct{}
}

func (b *blockedDownloadBody) Read(p []byte) (int, error) {
	close(b.started)
	<-b.resume
	return copy(p, "incomplete"), io.ErrUnexpectedEOF
}

func (*blockedDownloadBody) Close() error { return nil }

func TestDownloadFileConcurrentFailurePreservesSuccessfulDownload(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "rules.srs")
	if err := os.WriteFile(path, []byte("known good"), 0600); err != nil {
		t.Fatal(err)
	}
	body := &blockedDownloadBody{make(chan struct{}), make(chan struct{})}
	useDownloadTransport(t, func(req *http.Request) (*http.Response, error) {
		if req.URL.Query().Get("fail") == "1" {
			return downloadResponse(200, body), nil
		}
		return downloadResponse(200, io.NopCloser(strings.NewReader("complete"))), nil
	})
	result := make(chan error, 1)
	go func() { result <- DownloadFile("https://cache.invalid/rules.srs?fail=1", path, true) }()
	<-body.started
	// Report rather than abort here so the blocked download is always released.
	if got, err := os.ReadFile(path); err != nil || string(got) != "known good" {
		t.Errorf("unfinished download changed good cache: %q, %v", got, err)
	}
	if err := DownloadFile("https://cache.invalid/rules.srs", path, true); err != nil {
		t.Error(err)
	}
	close(body.resume)
	if err := <-result; err == nil {
		t.Fatal("expected interrupted download failure")
	}
	assertDownloadContents(t, path, "complete")
	assertDownloadDirectory(t, dir, "rules.srs")
}
