package boxmain

import (
	"github.com/sagernet/sing-box/common/srs"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json"
	"os"
)

type RuleSetType int

const (
	IpRuleSet RuleSetType = iota
	SiteRuleSet
)

func CompileRuleSet(path string, category string, ruleSetType RuleSetType, destPath string) error {
	var (
		content []byte
		err     error
	)

	if ruleSetType == IpRuleSet {
		content, err = geoipExport(path, category)
	} else {
		content, err = geositeExport(path, category)
	}
	if err != nil {
		return err
	}

	plainRuleSet, err := json.UnmarshalExtended[option.PlainRuleSetCompat](content)
	if err != nil {
		return err
	}
	if err != nil {
		return err
	}
	ruleSet, err := plainRuleSet.Upgrade()
	if err != nil {
		return err
	}

	outputFile, err := os.Create(destPath)
	if err != nil {
		return err
	}
	err = srs.Write(outputFile, ruleSet, plainRuleSet.Version)
	if err != nil {
		outputFile.Close()
		os.Remove(destPath)
		return err
	}
	outputFile.Close()
	return nil
}
