package internal

import (
	"errors"
	"net/http"
	"strings"
	"testing"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json/badoption"
)

func remoteRuleSet(tag, url string) option.RuleSet {
	return option.RuleSet{
		Type:          C.RuleSetTypeRemote,
		Tag:           badoption.Listable[string]{tag},
		Format:        C.RuleSetFormatBinary,
		RemoteOptions: option.RemoteRuleSet{URL: url},
	}
}

func TestModifyRulesetsCachesDownloadedRuleSet(t *testing.T) {
	previous := ruleset_cachedir
	ruleset_cachedir = t.TempDir()
	t.Cleanup(func() { ruleset_cachedir = previous })
	useDownloadTransport(t, func(*http.Request) (*http.Response, error) {
		return downloadResponse(200, http.NoBody), nil
	})

	url := "https://cache.invalid/geoip-ru.srs"
	opt := &option.Options{Route: &option.RouteOptions{RuleSet: []option.RuleSet{remoteRuleSet("geoip-ru", url)}}}
	ModifyRulesets(opt)

	got := opt.Route.RuleSet[0]
	if got.Type != C.RuleSetTypeLocal || got.LocalOptions.Path != urlToPath(url) {
		t.Fatalf("rule-set = %s %q, want local %q", got.Type, got.LocalOptions.Path, urlToPath(url))
	}
}

// A failed download used to produce a local rule-set with an empty path, which
// made sing-box read the working directory ("Incorrect function" on Windows) and
// abort the whole start.
func TestModifyRulesetsKeepsRemoteRuleSetWhenDownloadFails(t *testing.T) {
	previous := ruleset_cachedir
	ruleset_cachedir = t.TempDir()
	t.Cleanup(func() { ruleset_cachedir = previous })
	useDownloadTransport(t, func(*http.Request) (*http.Response, error) {
		return nil, errors.New("connection refused")
	})

	url := "https://cache.invalid/geosite-category-ru.srs"
	want := remoteRuleSet("geosite-category-ru", url)
	opt := &option.Options{Route: &option.RouteOptions{RuleSet: []option.RuleSet{want}}}
	ModifyRulesets(opt)

	got := opt.Route.RuleSet[0]
	if got.Type != C.RuleSetTypeRemote || got.RemoteOptions.URL != url || got.LocalOptions.Path != "" ||
		strings.Join(got.Tag, ",") != "geosite-category-ru" {
		t.Fatalf("rule-set = %+v, want unchanged remote %q", got, url)
	}
}
