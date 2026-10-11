package padding

import (
	"encoding/json"
	"os"
	"strings"
	"testing"

	"github.com/sagernet/sing-box/option"
)

// The only dependency API called is option decoding/normalization.
// Never start a core, construct a transport, dial, or access user settings.
func TestQtProductionEmission(t *testing.T) {
	data, err := os.ReadFile(os.Getenv("XHTTP_FIXTURES"))
	if err != nil {
		t.Fatal(err)
	}
	var fixtures []struct {
		Name     string
		Outbound struct{ Transport json.RawMessage }
		From, To int
		Error    string
	}
	if err := json.Unmarshal(data, &fixtures); err != nil {
		t.Fatal(err)
	}
	if len(fixtures) < 60 {
		t.Fatalf("missing fixtures: got %d", len(fixtures))
	}
	for _, fixture := range fixtures {
		t.Run(fixture.Name, func(t *testing.T) {
			var result option.V2RayTransportOptions
			err := json.Unmarshal(fixture.Outbound.Transport, &result)
			if fixture.Error != "" {
				if err == nil || !strings.Contains(err.Error(), fixture.Error) {
					t.Fatalf("expected rejection containing %q, got %v; JSON %s", fixture.Error, err, fixture.Outbound.Transport)
				}
				return
			}
			if err != nil {
				t.Fatalf("production JSON rejected: %v; JSON %s", err, fixture.Outbound.Transport)
			}
			if result.Type == "xhttp" {
				padding := result.XHTTPOptions.XPaddingBytes
				if padding.From != fixture.From || padding.To != fixture.To {
					t.Fatalf("decoded range %+v, want %d-%d", padding, fixture.From, fixture.To)
				}
			}
		})
	}
}

// Independent controls prove why an explicit emitter default is needed.
func TestPinnedCoreDefaultContract(t *testing.T) {
	var base option.V2RayXHTTPBaseOptions
	padding := base.GetNormalizedXPaddingBytes()
	if padding.From != 100 || padding.To != 1000 {
		t.Fatalf("core default changed: %+v", padding)
	}
	var transport option.V2RayTransportOptions
	err := json.Unmarshal([]byte(`{"type":"xhttp","mode":"auto"}`), &transport)
	if err == nil || !strings.Contains(err.Error(), "x_padding_bytes cannot be disabled") {
		t.Fatalf("pinned decoder's omitted-padding contract changed: %v", err)
	}
}
