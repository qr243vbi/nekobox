package process

import (
	"os"
	"path/filepath"
	"testing"
)

func TestIndependentCoreConfigFiles(t *testing.T) {
	dir := filepath.Join(t.TempDir(), "config with spaces")
	if err := os.Mkdir(dir, 0700); err != nil {
		t.Fatal(err)
	}
	first := Config{Path: "xray.exe", Args: "run -format json -c %s", Content: `{"profile":"main"}`, Directory: dir}
	second := first
	second.Content = `{"profile":"routing"}`
	for index, config := range []Config{first, second} {
		args, path, err := prepareConfig(config, index)
		if err != nil {
			t.Fatal(err)
		}
		name := "extra.json"
		if index == 1 {
			name = "extra-1.json"
		}
		if filepath.Base(path) != name || args[len(args)-1] != path || len(args) != 5 {
			t.Fatalf("invalid config filename or spaced argument: %v %q", args, path)
		}
	}
	for name, expected := range map[string]string{"extra.json": first.Content, "extra-1.json": second.Content} {
		actual, err := os.ReadFile(filepath.Join(dir, name))
		if err != nil || string(actual) != expected {
			t.Fatalf("one external core overwrote another: %s: %v", name, err)
		}
	}
	other := Config{Path: "custom-core.exe", Args: "--config %s", Content: "legacy-format", Directory: dir}
	_, path, err := prepareConfig(other, 0)
	if err != nil || filepath.Base(path) != "extra.conf" {
		t.Fatalf("legacy custom core filename changed: %q %v", path, err)
	}
}

func TestRejectInvalidCoreConfigBeforeStartingProcess(t *testing.T) {
	if _, _, err := prepareConfig(Config{}, 0); err == nil {
		t.Fatal("empty executable accepted")
	}
	if _, _, err := prepareConfig(Config{Path: "unused.exe", Args: `"unterminated`}, 0); err == nil {
		t.Fatal("invalid arguments accepted")
	}
}
