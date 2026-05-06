package rule

import "github.com/sagernet/sing-box/adapter"

type ruleMatchState uint8

const (
	ruleMatchSourceAddress ruleMatchState = 1 << iota
	ruleMatchSourcePort
	ruleMatchDestinationAddress
	ruleMatchDestinationPort
)

type ruleMatchStateSet uint16

func singleRuleMatchState(state ruleMatchState) ruleMatchStateSet {
	return 1 << state
}

func emptyRuleMatchState() ruleMatchStateSet {
	return singleRuleMatchState(0)
}

func (s ruleMatchStateSet) isEmpty() bool {
	return s == 0
}

func (s ruleMatchStateSet) contains(state ruleMatchState) bool {
	return s&(1<<state) != 0
}

func (s ruleMatchStateSet) add(state ruleMatchState) ruleMatchStateSet {
	return s | singleRuleMatchState(state)
}

func (s ruleMatchStateSet) merge(other ruleMatchStateSet) ruleMatchStateSet {
	return s | other
}

func (s ruleMatchStateSet) combine(other ruleMatchStateSet) ruleMatchStateSet {
	if s.isEmpty() || other.isEmpty() {
		return 0
	}
	var combined ruleMatchStateSet
	for left := ruleMatchState(0); left < 16; left++ {
		if !s.contains(left) {
			continue
		}
		for right := ruleMatchState(0); right < 16; right++ {
			if !other.contains(right) {
				continue
			}
			combined = combined.add(left | right)
		}
	}
	return combined
}

func (s ruleMatchStateSet) withBase(base ruleMatchState) ruleMatchStateSet {
	if s.isEmpty() {
		return 0
	}
	var withBase ruleMatchStateSet
	for state := ruleMatchState(0); state < 16; state++ {
		if !s.contains(state) {
			continue
		}
		withBase = withBase.add(state | base)
	}
	return withBase
}

func (s ruleMatchStateSet) filter(allowed func(ruleMatchState) bool) ruleMatchStateSet {
	var filtered ruleMatchStateSet
	for state := ruleMatchState(0); state < 16; state++ {
		if !s.contains(state) {
			continue
		}
		if allowed(state) {
			filtered = filtered.add(state)
		}
	}
	return filtered
}

type ruleStateMatcher interface {
	matchStates(metadata *adapter.InboundContext) ruleMatchStateSet
}

type ruleStateMatcherWithBase interface {
	matchStatesWithBase(metadata *adapter.InboundContext, base ruleMatchState) ruleMatchStateSet
}

func matchHeadlessRuleStates(rule adapter.HeadlessRule, metadata *adapter.InboundContext) ruleMatchStateSet {
	return matchHeadlessRuleStatesWithBase(rule, metadata, 0)
}

func matchHeadlessRuleStatesWithBase(rule adapter.HeadlessRule, metadata *adapter.InboundContext, base ruleMatchState) ruleMatchStateSet {
	if matcher, isStateMatcher := rule.(ruleStateMatcherWithBase); isStateMatcher {
		return matcher.matchStatesWithBase(metadata, base)
	}
	if matcher, isStateMatcher := rule.(ruleStateMatcher); isStateMatcher {
		return matcher.matchStates(metadata).withBase(base)
	}
	if rule.Match(metadata) {
		return emptyRuleMatchState().withBase(base)
	}
	return 0
}

func matchRuleItemStates(item RuleItem, metadata *adapter.InboundContext) ruleMatchStateSet {
	return matchRuleItemStatesWithBase(item, metadata, 0)
}

func matchRuleItemStatesWithBase(item RuleItem, metadata *adapter.InboundContext, base ruleMatchState) ruleMatchStateSet {
	if matcher, isStateMatcher := item.(ruleStateMatcherWithBase); isStateMatcher {
		return matcher.matchStatesWithBase(metadata, base)
	}
	if matcher, isStateMatcher := item.(ruleStateMatcher); isStateMatcher {
		return matcher.matchStates(metadata).withBase(base)
	}
	if item.Match(metadata) {
		return emptyRuleMatchState().withBase(base)
	}
	return 0
}
