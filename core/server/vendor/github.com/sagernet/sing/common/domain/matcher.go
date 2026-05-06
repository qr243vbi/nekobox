package domain

import (
	"sort"
	"unicode/utf8"

	"github.com/sagernet/sing/common/varbin"
)

const (
	prefixLabel = '\r'
	rootLabel   = '\n'
)

type Matcher struct {
	set *succinctSet
}

func NewMatcher(domains []string, domainSuffix []string, generateLegacy bool) *Matcher {
	domainList := make([]string, 0, len(domains)+2*len(domainSuffix))
	seen := make(map[string]bool, len(domainList))
	for _, domain := range domainSuffix {
		if seen[domain] {
			continue
		}
		seen[domain] = true
		if domain[0] == '.' {
			domainList = append(domainList, reverseDomain(string(prefixLabel)+domain))
		} else if generateLegacy {
			domainList = append(domainList, reverseDomain(domain))
			suffixDomain := "." + domain
			if !seen[suffixDomain] {
				seen[suffixDomain] = true
				domainList = append(domainList, reverseDomain(string(prefixLabel)+suffixDomain))
			}
		} else {
			domainList = append(domainList, reverseDomain(string(rootLabel)+domain))
		}
	}
	for _, domain := range domains {
		if seen[domain] {
			continue
		}
		seen[domain] = true
		domainList = append(domainList, reverseDomain(domain))
	}
	sort.Strings(domainList)
	return &Matcher{newSuccinctSet(domainList)}
}

func ReadMatcher(reader varbin.Reader) (*Matcher, error) {
	set, err := readSuccinctSet(reader)
	if err != nil {
		return nil, err
	}
	return &Matcher{set}, nil
}

func (m *Matcher) Write(writer varbin.Writer) error {
	return m.set.Write(writer)
}

func (m *Matcher) Match(domain string) bool {
	return m.has(reverseDomain(domain))
}

func (m *Matcher) has(key string) bool {
	var nodeId, bmIdx int
	for i := 0; i < len(key); i++ {
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
		if nextLabel == prefixLabel || nextLabel == rootLabel {
			return true
		}
	}
}

func (m *Matcher) Dump() (domainList []string, prefixList []string) {
	domainMap := make(map[string]bool)
	prefixMap := make(map[string]bool)
	for _, key := range m.set.keys() {
		key = reverseDomain(key)
		if key[0] == prefixLabel {
			prefixMap[key[1:]] = true
		} else if key[0] == rootLabel {
			prefixList = append(prefixList, key[1:])
		} else {
			domainMap[key] = true
		}
	}
	for rawPrefix := range prefixMap {
		if rawPrefix[0] == '.' {
			if rootDomain := rawPrefix[1:]; domainMap[rootDomain] {
				delete(domainMap, rootDomain)
				prefixList = append(prefixList, rootDomain)
				continue
			}
		}
		prefixList = append(prefixList, rawPrefix)
	}
	for domain := range domainMap {
		domainList = append(domainList, domain)
	}
	sort.Strings(domainList)
	sort.Strings(prefixList)
	return domainList, prefixList
}

func reverseDomain(domain string) string {
	l := len(domain)
	b := make([]byte, l)
	for i := 0; i < l; {
		r, n := utf8.DecodeRuneInString(domain[i:])
		i += n
		utf8.EncodeRune(b[l-i:], r)
	}
	return string(b)
}
