package internal

import (
	"fmt"
	"os"
	"net/http"
	"io"
	"time"
	"path/filepath"
	"strings"
	"github.com/sagernet/sing-box/option"
	C "github.com/sagernet/sing-box/constant"
)

const (
	Gb = 1000 * Mb
	Mb = 1000 * Kb
	Kb = 1000
)

var ruleset_cachedir string

func SetRulesetCachedir(v string) bool {
	ruleset_cachedir = v
	return true
}

func DownloadFile(url, targetPath string) error {
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
	resp, err := http.Get(url)
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

func CacheHttpBool(url string, s * bool) string {
	path := urlToPath(url)
	if !fileExists(path) {
		DownloadFile(url, path)
		if s != nil {
			*s = true
		}
	} else if s != nil {
		*s = false
	}
	return path
}

func CacheHttp(url string) string {
	return CacheHttpBool(url, nil)
}

func cacheRuleSet(url string, format string, tag string) option.RuleSet{
	var ruleset option.RuleSet
	ruleset.Tag = tag
	ruleset.Type = C.RuleSetTypeLocal
	ruleset.Format = format
	path := CacheHttp(url)
	ruleset.LocalOptions.Path = path
	return ruleset
}

func ModifyRulesets(opt * option.Options){
	if (ruleset_cachedir == ""){
		return
	}
	for u, i := range opt.Route.RuleSet {
		if (i.Type == C.RuleSetTypeRemote){
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
	return bytes / elapsed
}
