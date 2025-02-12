package boxmain

import (
	"bytes"
	"net"
	"strings"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"

	"github.com/oschwald/maxminddb-golang"
)

func geoipExport(path string, countryCode string) ([]byte, error) {
	if err := geoipPreRun(path); err != nil {
		return nil, err
	}

	networks := geoipReader.Networks(maxminddb.SkipAliasedNetworks)
	countryMap := make(map[string][]*net.IPNet)
	var (
		ipNet           *net.IPNet
		nextCountryCode string
		err             error
	)
	for networks.Next() {
		ipNet, err = networks.Network(&nextCountryCode)
		if err != nil {
			return nil, err
		}
		countryMap[nextCountryCode] = append(countryMap[nextCountryCode], ipNet)
	}
	ipNets := countryMap[strings.ToLower(countryCode)]
	if len(ipNets) == 0 {
		return nil, E.New("country code not found: ", countryCode)
	}

	outputWriter := &bytes.Buffer{}

	encoder := json.NewEncoder(outputWriter)
	encoder.SetIndent("", "  ")
	var headlessRule option.DefaultHeadlessRule
	headlessRule.IPCIDR = make([]string, 0, len(ipNets))
	for _, cidr := range ipNets {
		headlessRule.IPCIDR = append(headlessRule.IPCIDR, cidr.String())
	}
	var plainRuleSet option.PlainRuleSetCompat
	plainRuleSet.Version = C.RuleSetVersion1
	plainRuleSet.Options.Rules = []option.HeadlessRule{
		{
			Type:           C.RuleTypeDefault,
			DefaultOptions: headlessRule,
		},
	}

	err = encoder.Encode(plainRuleSet)
	return outputWriter.Bytes(), err
}
