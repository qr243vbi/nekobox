package internal

import (
	"context"
	"fmt"
	"io"
	"log"
	"nekobox_core/internal/boxbox"
	"nekobox_core/internal/process"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"

	"net"

	"github.com/sagernet/sing-box/common/settings"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json/badoption"
	"github.com/sagernet/sing/common/metadata"
)

const (
	Gb = 1000 * Mb
	Mb = 1000 * Kb
	Kb = 1000
)

var ruleset_cachedir string

var BoxInstance *boxbox.Box
var ExtraProcess *process.Process
var NeedUnsetDNS bool

var SystemProxyController settings.SystemProxy
var SystemProxyAddr string
var SystemProxyPort uint16
var SystemProxySupportSOCKS bool

var InstanceCancel context.CancelFunc
var Debug bool

func IsSystemProxyEnabled() bool {
	return SystemProxyController != nil && SystemProxyController.IsEnabled()
}

func ResetSystemProxy() error {
	if IsSystemProxyEnabled() {
		return SystemProxyController.Disable()
	}
	return nil
}

func SetSystemProxy(ctx context.Context, serverAddr string, serverPort uint16, supportSOCKS bool) error {
	if serverAddr == "" {
		return nil
	}

	if SystemProxyController != nil {
		if serverAddr == SystemProxyAddr && serverPort == SystemProxyPort && supportSOCKS == SystemProxySupportSOCKS {
			if SystemProxyController.IsEnabled() {
				return nil
			}
		}

		if err := SystemProxyController.Disable(); err != nil {
			return fmt.Errorf("disable previous system proxy: %w", err)
		}
	}

	SystemProxyAddr = serverAddr
	SystemProxyPort = serverPort
	SystemProxySupportSOCKS = supportSOCKS

	addr := metadata.ParseSocksaddrHostPort(serverAddr, serverPort)
	proxy, err := settings.NewSystemProxy(ctx, addr, supportSOCKS, nil)
	if err != nil {
		return fmt.Errorf("create system proxy: %w", err)
	}

	SystemProxyController = proxy
	if err := SystemProxyController.Enable(); err != nil {
		return fmt.Errorf("enable system proxy: %w", err)
	}
	return nil
}

func SetRulesetCachedir(v string) bool {
	ruleset_cachedir = v
	return true
}

func BoxCreateHttpClient(instance *boxbox.Box) *http.Client {
	if instance == nil {
		return &http.Client{}
	}

	outbound := instance.Outbound()
	if outbound == nil || outbound.Default() == nil {
		return &http.Client{}
	}

	return &http.Client{
		Transport: &http.Transport{
			DialContext: func(ctx context.Context, network string, addr string) (net.Conn, error) {
				return outbound.Default().DialContext(ctx, "tcp", metadata.ParseSocksaddr(addr))
			},
		},
	}
}

func DownloadFile(originalURL, targetPath string, use_default_outbound bool) error {
	dir := filepath.Dir(targetPath)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return fmt.Errorf("failed to create destination directory %q: %w", dir, err)
	}

	outFile, err := os.Create(targetPath)
	if err != nil {
		return fmt.Errorf("failed to create file: %w", err)
	}
	defer outFile.Close()

	client := &http.Client{}
	if !use_default_outbound && BoxInstance != nil {
		client = BoxCreateHttpClient(BoxInstance)
	}

	parsedURL, err := url.Parse(originalURL)
	if err != nil {
		return fmt.Errorf("error parsing URL: %w", err)
	}

	var username, password string
	if parsedURL.User != nil {
		username = parsedURL.User.Username()
		password, _ = parsedURL.User.Password()
		parsedURL.User = nil
	}

	cleanedURL := parsedURL.String()

	req, err := http.NewRequest("GET", cleanedURL, nil)
	if err != nil {
		return fmt.Errorf("error creating request: %w", err)
	}
	if username != "" && password != "" {
		req.SetBasicAuth(username, password)
	}

	resp, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("failed to download file: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < http.StatusOK || resp.StatusCode >= http.StatusMultipleChoices {
		return fmt.Errorf("download failed: %s", resp.Status)
	}

	if _, err = io.Copy(outFile, resp.Body); err != nil {
		return fmt.Errorf("failed to save file: %w", err)
	}
	return nil
}

func urlToPath(url string) string {
	url = strings.Replace(url, ":/", "/", 1)
	url = strings.ReplaceAll(url, "_", "_0_")
	url = strings.ReplaceAll(url, ":", "_1_")
	url = strings.ReplaceAll(url, "@", "_2_")
	url = strings.ReplaceAll(url, "?", "_3_")
	url = strings.ReplaceAll(url, "=", "_4_")
	url = strings.ReplaceAll(url, "&", "_5_")
	url = strings.ReplaceAll(url, "\"", "_6_")
	url = strings.ReplaceAll(url, "'", "_7_")
	url = strings.ReplaceAll(url, "*", "_8_")
	url = filepath.Clean(filepath.FromSlash(url))
	url = filepath.Clean(filepath.Join(ruleset_cachedir, url))
	return url
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return !os.IsNotExist(err)
}

func CacheHttpBool(url string, use_default_outbound bool, s *bool) string {
	if url == "" {
		if s != nil {
			*s = false
		}
		return ""
	}

	path := urlToPath(url)
	if !fileExists(path) {
		log.Printf("Downloading %s %s", url, func() string {
			if use_default_outbound {
				return "with proxy"
			}
			return "without proxy"
		}())

		if err := DownloadFile(url, path, use_default_outbound); err != nil {
			log.Printf("Error while downloading %s: %v", url, err)
			if s != nil {
				*s = false
			}
			return ""
		}

		if s != nil {
			*s = true
		}
	} else if s != nil {
		log.Printf("Cached %s", url)
		*s = false
	}
	return path
}

func CacheHttp(url string, use_default_outbound bool) string {
	return CacheHttpBool(url, use_default_outbound, nil)
}

func cacheRuleSet(url string, format string, tag badoption.Listable[string]) option.RuleSet {
	var ruleset option.RuleSet
	ruleset.Tag = tag
	ruleset.Type = C.RuleSetTypeLocal
	ruleset.Format = format
	ruleset.LocalOptions.Path = CacheHttp(url, false)
	return ruleset
}

func ClearRulesets() {
	if ruleset_cachedir == "" {
		return
	}
	paths := []string{
		"ftps",
		"ftp",
		"http",
		"https",
	}

	for _, path := range paths {
		_ = os.RemoveAll(filepath.Clean(filepath.Join(ruleset_cachedir, path)))
	}
}

func ModifyRulesets(opt *option.Options) {
	if ruleset_cachedir == "" || opt == nil || opt.Route == nil {
		return
	}

	for u, i := range opt.Route.RuleSet {
		if i.Type == C.RuleSetTypeRemote && i.RemoteOptions.URL != "" {
			url := i.RemoteOptions.URL
			opt.Route.RuleSet[u] = cacheRuleSet(url, i.Format, i.Tag)
		}
	}
}

func GetRulesetCachedir() string {
	return ruleset_cachedir
}

func BrateToStr(brate float64) string {
	brate *= 8
	if brate >= Gb {
		return fmt.Sprintf("%.2f%s", brate/Gb, "Gbps")
	}
	if brate >= Mb {
		return fmt.Sprintf("%.2f%s", brate/Mb, "Mbps")
	}
	return fmt.Sprintf("%.2f%s", brate/Kb, "Kbps")
}

func CalculateBRate(bytes float64, startTime time.Time) float64 {
	elapsed := time.Since(startTime).Seconds()
	if elapsed <= 0 {
		return 0
	}
	return bytes / elapsed
}
