package domain

import (
	"sort"
	"strings"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/varbin"
)

const (
	anyLabel    = '*'
	suffixLabel = '\b'
)

type AdGuardMatcher struct {
	set *succinctSet
}

func NewAdGuardMatcher(ruleLines []string) *AdGuardMatcher {
	ruleList := make([]string, 0, len(ruleLines))
	for _, ruleLine := range ruleLines {
		var (
			isSuffix bool // ||
			hasStart bool // |
			hasEnd   bool // ^
		)
		if strings.HasPrefix(ruleLine, "||") {
			ruleLine = ruleLine[2:]
			isSuffix = true
		} else if strings.HasPrefix(ruleLine, "|") {
			ruleLine = ruleLine[1:]
			hasStart = true
		}
		if strings.HasSuffix(ruleLine, "^") {
			ruleLine = ruleLine[:len(ruleLine)-1]
			hasEnd = true
		}
		if isSuffix {
			ruleLine = string(rootLabel) + ruleLine
		} else if !hasStart {
			ruleLine = string(prefixLabel) + ruleLine
		}
		if !hasEnd {
			if strings.HasSuffix(ruleLine, ".") {
				ruleLine = ruleLine[:len(ruleLine)-1]
			}
			ruleLine += string(suffixLabel)
		}
		ruleList = append(ruleList, reverseDomain(ruleLine))
	}
	ruleList = common.Uniq(ruleList)
	sort.Strings(ruleList)
	return &AdGuardMatcher{newSuccinctSet(ruleList)}
}

func ReadAdGuardMatcher(reader varbin.Reader) (*AdGuardMatcher, error) {
	set, err := readSuccinctSet(reader)
	if err != nil {
		return nil, err
	}
	return &AdGuardMatcher{set}, nil
}

func (m *AdGuardMatcher) Write(writer varbin.Writer) error {
	return m.set.Write(writer)
}

func (m *AdGuardMatcher) Match(domain string) bool {
	key := reverseDomain(domain)
	if m.has([]byte(key), 0, 0) {
		return true
	}
	for {
		if m.has([]byte(string(suffixLabel)+key), 0, 0) {
			return true
		}
		idx := strings.IndexByte(key, '.')
		if idx == -1 {
			return false
		}
		key = key[idx+1:]
	}
}

func (m *AdGuardMatcher) has(key []byte, nodeId, bmIdx int) bool {
	return m.hasWithDepth(key, nodeId, bmIdx, 0)
}

func (m *AdGuardMatcher) hasWithDepth(key []byte, nodeId, bmIdx int, depth int) bool {
	const maxDepth = 100
	if depth > maxDepth {
		return false
	}

	keyLen := len(key)
	for i := 0; i < keyLen; i++ {
		currentChar := key[i]
		for ; ; bmIdx++ {
			if getBit(m.set.labelBitmap, bmIdx) != 0 {
				return false
			}
			nextLabel := m.set.labels[bmIdx-nodeId]
			if nextLabel == prefixLabel {
				return true
			}
			if nextLabel == rootLabel {
				nextNodeId := countZeros(m.set.labelBitmap, m.set.ranks, bmIdx+1)
				hasNext := getBit(m.set.leaves, nextNodeId) != 0
				if currentChar == '.' && hasNext {
					return true
				}
			}
			if nextLabel == currentChar {
				break
			}
			if nextLabel == anyLabel || nextLabel == suffixLabel {
				nextNodeId := countZeros(m.set.labelBitmap, m.set.ranks, bmIdx+1)
				nextBmIdx := selectIthOne(m.set.labelBitmap, m.set.ranks, m.set.selects, nextNodeId-1) + 1

				if m.hasWithDepth(key[i:], nextNodeId, nextBmIdx, depth+1) {
					return true
				}

				for j := i + 1; j <= keyLen; j++ {
					if m.hasWithDepth(key[j:], nextNodeId, nextBmIdx, depth+1) {
						return true
					}
				}
			}
		}
		nodeId = countZeros(m.set.labelBitmap, m.set.ranks, bmIdx+1)
		bmIdx = selectIthOne(m.set.labelBitmap, m.set.ranks, m.set.selects, nodeId-1) + 1
	}
	if getBit(m.set.leaves, nodeId) != 0 {
		return true
	}
	for ; ; bmIdx++ {
		if getBit(m.set.labelBitmap, bmIdx) != 0 {
			return false
		}
		nextLabel := m.set.labels[bmIdx-nodeId]
		if nextLabel == prefixLabel || nextLabel == rootLabel || nextLabel == suffixLabel {
			return true
		}
		if nextLabel == anyLabel {
			nextNodeId := countZeros(m.set.labelBitmap, m.set.ranks, bmIdx+1)
			nextBmIdx := selectIthOne(m.set.labelBitmap, m.set.ranks, m.set.selects, nextNodeId-1) + 1
			return m.hasWithDepth([]byte{}, nextNodeId, nextBmIdx, depth+1)
		}
	}
}

func (m *AdGuardMatcher) Dump() (ruleLines []string) {
	for _, key := range m.set.keys() {
		key = reverseDomain(key)
		var (
			isSuffix bool
			hasStart bool
			hasEnd   bool
		)
		if key[0] == prefixLabel {
			key = key[1:]
		} else if key[0] == rootLabel {
			key = key[1:]
			isSuffix = true
		} else {
			hasStart = true
		}
		if key[len(key)-1] == suffixLabel {
			key = key[:len(key)-1]
		} else {
			hasEnd = true
		}
		if isSuffix {
			key = "||" + key
		} else if hasStart {
			key = "|" + key
		}
		if hasEnd {
			key += "^"
		}
		ruleLines = append(ruleLines, key)
	}
	return
}
