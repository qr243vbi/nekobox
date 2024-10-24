package boxmain

import (
	"bytes"
	"github.com/sagernet/sing-box/common/geosite"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json"
)

func geositeExport(path string, category string) ([]byte, error) {
	if err := geositePreRun(path); err != nil {
		return nil, err
	}

	sourceSet, err := geositeReader.Read(category)
	if err != nil {
		return nil, err
	}

	outputWriter := &bytes.Buffer{}

	encoder := json.NewEncoder(outputWriter)
	encoder.SetIndent("", "  ")
	var headlessRule option.DefaultHeadlessRule
	defaultRule := geosite.Compile(sourceSet)
	headlessRule.Domain = defaultRule.Domain
	headlessRule.DomainSuffix = defaultRule.DomainSuffix
	headlessRule.DomainKeyword = defaultRule.DomainKeyword
	headlessRule.DomainRegex = defaultRule.DomainRegex
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
