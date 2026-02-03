package internal

import (
	"context"
	"fmt"
	"io"
	"log"
	"nekobox_core/internal/boxbox"
	"nekobox_core/internal/process"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"net"
	"github.com/sagernet/sing-box/common/settings"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
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
var SystemProxyAddr metadata.Socksaddr
var InstanceCancel context.CancelFunc
var Debug bool

func SetRulesetCachedir(v string) bool {
	ruleset_cachedir = v
	return true
}

func BoxCreateHttpClient(instance *boxbox.Box) * http.Client {
	if (instance == nil){
		return &http.Client{};
	}
	outbound := BoxInstance.Outbound().Default()
	client := &http.Client{
		Transport: &http.Transport{
			DialContext: func(ctx context.Context, network string, addr string) (net.Conn, error) {
				return outbound.DialContext(ctx, "tcp", metadata.ParseSocksaddr(addr))
			},
		},
	}
	return client;
}

func DownloadFile(url, targetPath string, use_default_outbound bool) error {
	// Create all necessary directories for the target path
	dir := filepath.Dir(targetPath)
	err := os.MkdirAll(dir, 0755)
	// Open the file for writing
	outFile, err := os.Create(targetPath)
	if err != nil {
		return fmt.Errorf("failed to create file: %v", err)
	}
	defer outFile.Close()
	// Download the file
	var client *http.Client

	if (use_default_outbound){
		goto def_cli
	} else {
		if (BoxInstance == nil){
			goto def_cli
		} else {
			client = BoxCreateHttpClient(BoxInstance)
			goto skip_def_cli
		}
	}
	def_cli:

	client = &http.Client{}
	skip_def_cli:

	resp, err := client.Get(url)
	if err != nil {
		return fmt.Errorf("failed to download file: %v", err)
	}
	defer resp.Body.Close()
	// Copy the downloaded content to the file
	_, err = io.Copy(outFile, resp.Body)
	if err != nil {
		return fmt.Errorf("failed to save file: %v", err)
	}
	return nil
}

func urlToPath(url string) string {
	result := strings.Replace(url, ":/", "/", 1)
	result = filepath.Clean(filepath.FromSlash(result))
	result = filepath.Clean(filepath.Join(ruleset_cachedir, result))
	return result
}

func fileExists(path string) bool {
	_, err := os.Stat(path)
	return !os.IsNotExist(err)
}

func CacheHttpBool(url string, use_default_outbound bool, s * bool) string {
	if (url == ""){
		*s = false
		return ""
	}
	path := urlToPath(url)
	if !fileExists(path) {
		log.Printf("Downloading %s %s", url, func() string {
			if use_default_outbound {
				return "with proxy"
			}
			return "without proxy"
		}() )
		err := DownloadFile(url, path, use_default_outbound)
		if (err != nil){
			log.Fatalf("Error while downloading: %s", err.Error())
		}
		if s != nil {
			*s = true
		}
	} else if s != nil {
		log.Printf("Cached %s", url);
		*s = false
	}
	return path
}

func CacheHttp(url string, use_default_outbound bool) string {
	return CacheHttpBool(url, use_default_outbound, nil)
}

func cacheRuleSet(url string, format string, tag string) option.RuleSet{
	var ruleset option.RuleSet
	ruleset.Tag = tag
	ruleset.Type = C.RuleSetTypeLocal
	ruleset.Format = format
	ruleset.LocalOptions.Path = CacheHttp(url, false)
	return ruleset
}

func ClearRulesets(){
	paths := []string{
		"ftps",
		"ftp",
		"http",
		"https",
	}

	for _, path := range paths {
		os.RemoveAll(filepath.Clean(filepath.Join(ruleset_cachedir, path)))
	}
}

func ModifyRulesets(opt * option.Options){
	if (ruleset_cachedir == ""){
		return
	}
	if (opt.Route != nil){
		for u, i := range opt.Route.RuleSet {
			if (i.Type == C.RuleSetTypeRemote){
				url := i.RemoteOptions.URL
				opt.Route.RuleSet[u] = cacheRuleSet(url, i.Format, i.Tag)
			}
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
	return bytes / elapsed
}
