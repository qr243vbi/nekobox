package boxmain

// A remote rule-set that NekoBox could not cache must not stop the core from
// starting: sing-box fetches it itself. The cache download fails here the way
// it does when HTTP(S)_PROXY points at an inbound that is not listening yet.

import (
	"bytes"
	"errors"
	"fmt"
	"net/http"
	"net/http/httptest"
	"sync/atomic"
	"testing"

	"nekobox_core/internal"

	"github.com/sagernet/sing-box/common/srs"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json/badoption"
)

type refusedTransport struct{}

func (refusedTransport) RoundTrip(*http.Request) (*http.Response, error) {
	return nil, errors.New("connection refused (proxy from the environment is not listening)")
}

func TestStartWhenRuleSetCacheDownloadFails(t *testing.T) {
	var body bytes.Buffer
	ruleSet := option.PlainRuleSet{Rules: []option.HeadlessRule{{
		Type:           C.RuleTypeDefault,
		DefaultOptions: option.DefaultHeadlessRule{Domain: badoption.Listable[string]{"example.com"}},
	}}}
	if err := srs.Write(&body, ruleSet, C.RuleSetVersionCurrent); err != nil {
		t.Fatal(err)
	}
	var fetched atomic.Int32
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fetched.Add(1)
		_, _ = w.Write(body.Bytes())
	}))
	defer server.Close()

	previousDir := internal.GetRulesetCachedir()
	internal.SetRulesetCachedir(t.TempDir())
	defer internal.SetRulesetCachedir(previousDir)
	previous := http.DefaultTransport
	http.DefaultTransport = refusedTransport{}
	defer func() { http.DefaultTransport = previous }()

	config := fmt.Sprintf(`{"log":{"level":"warn"},
 "outbounds":[{"type":"direct","tag":"direct"}],
 "route":{"rule_set":[{"type":"remote","tag":"geosite-test","format":"binary","url":%q}],
          "rules":[{"rule_set":"geosite-test","action":"route","outbound":"direct"}],
          "final":"direct"}}`, server.URL+"/geosite-test.srs")
	instance, cancel, err := Create([]byte(config))
	if err != nil {
		t.Fatalf("start failed: %v", err)
	}
	defer cancel()
	defer instance.Close()
	t.Logf("started; sing-box fetched the rule-set itself %d time(s)", fetched.Load())
	if fetched.Load() == 0 {
		t.Fatal("the rule-set was never fetched")
	}
}
