package process

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/google/shlex"
	E "github.com/sagernet/sing/common/exceptions"
)

type Config struct {
	Path, Args, Content, Directory string
	NoOut                          bool
}

// prepareConfig keeps the legacy primary filename and separates auxiliary
// configs, including two Xray processes with different configurations.
func prepareConfig(config Config, index int) ([]string, string, error) {
	if config.Path == "" || index < 0 {
		return nil, "", E.New("invalid external core specification")
	}
	args, err := shlex.Split(config.Args)
	if err != nil {
		return nil, "", E.Cause(err, "parse external core args")
	}
	if config.Content == "" {
		return args, "", nil
	}
	base := "extra"
	if index > 0 {
		base += "-" + strconv.Itoa(index)
	}
	ext := ".conf"
	binary := filepath.Base(config.Path)
	if strings.EqualFold(binary, "xray") || strings.EqualFold(binary, "xray.exe") {
		ext = ".json"
	}
	path := filepath.Join(config.Directory, base+ext)
	for i := range args {
		args[i] = strings.ReplaceAll(args[i], "%s", path)
	}
	if err := os.WriteFile(path, []byte(config.Content), 0600); err != nil {
		return nil, "", E.Cause(err, "write external core config")
	}
	return args, path, nil
}

func StartConfigured(config Config, index int) (*Process, error) {
	args, _, err := prepareConfig(config, index)
	if err != nil {
		return nil, err
	}
	child := NewProcess(config.Path, args, config.NoOut)
	if err = child.Start(); err != nil {
		return nil, err
	}
	if addresses := LocalInboundAddrs(config.Content); len(addresses) > 0 {
		if err = child.WaitListening(addresses, 10*time.Second); err != nil {
			child.Stop()
			return nil, E.Cause(err, "start external core")
		}
	}
	return child, nil
}
