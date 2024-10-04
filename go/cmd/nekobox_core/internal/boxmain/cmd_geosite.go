package boxmain

import (
	"github.com/sagernet/sing-box/common/geosite"
	E "github.com/sagernet/sing/common/exceptions"
)

var (
	geositeReader   *geosite.Reader
	geositeCodeList []string
)

func geositePreRun() error {
	reader, codeList, err := geosite.Open("../geosite.db") // currentDir is the config folder
	if err != nil {
		return E.Cause(err, "open geosite file ")
	}
	geositeReader = reader
	geositeCodeList = codeList
	return nil
}
