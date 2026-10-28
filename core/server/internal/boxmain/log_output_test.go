package boxmain

// sing-box messages reach the GUI's log tab through the core's stderr. A config
// without "log.output" must therefore log to os.Stderr, not drop the messages.

import (
	"os"
	"strings"
	"testing"
)

func TestLogGoesToStderrByDefault(t *testing.T) {
	capture, err := os.CreateTemp(t.TempDir(), "stderr")
	if err != nil {
		t.Fatal(err)
	}
	defer capture.Close()
	previous := os.Stderr
	os.Stderr = capture
	defer func() { os.Stderr = previous }()

	instance, cancel, err := Create([]byte(`{"log":{"level":"info"},
 "outbounds":[{"type":"direct","tag":"direct"}]}`))
	if err != nil {
		t.Fatalf("start failed: %v", err)
	}
	instance.Close()
	cancel()
	os.Stderr = previous

	written, err := os.ReadFile(capture.Name())
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(written), "sing-box started") {
		t.Fatalf("stderr does not have the start message, got:\n%s", written)
	}
}
