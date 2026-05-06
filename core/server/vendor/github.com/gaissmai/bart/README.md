# package bart

![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/gaissmai/bart)
[![Go Reference](https://pkg.go.dev/badge/github.com/gaissmai/bart.svg)](https://pkg.go.dev/github.com/gaissmai/bart#section-documentation)
[![Mentioned in Awesome Go](https://awesome.re/mentioned-badge-flat.svg)](https://github.com/avelino/awesome-go)
[![CI](https://github.com/gaissmai/bart/actions/workflows/go.yml/badge.svg)](https://github.com/gaissmai/bart/actions/workflows/go.yml)
[![Coverage Status](https://coveralls.io/repos/github/gaissmai/bart/badge.svg)](https://coveralls.io/github/gaissmai/bart)
[![Go Report Card](https://goreportcard.com/badge/github.com/gaissmai/bart)](https://goreportcard.com/report/github.com/gaissmai/bart)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)

## Overview

`package bart` provides a Balanced-Routing-Table (BART).

BART is balanced in terms of memory usage and lookup time
for the longest-prefix match.

BART is a multibit-trie with fixed stride length of 8 bits,
using the _baseIndex_ function from the ART algorithm to
build the complete-binary-tree (CBT) of prefixes for each stride.

The CBT is implemented as a bit-vector, backtracking is just
a matter of fast cache friendly bitmask operations.

The Table is implemented with popcount compressed sparse arrays
together with path compression. This reduces storage consumption
by almost two orders of magnitude in comparison to ART with
comparable or even better lookup times for longest prefix match.

The algorithm is also excellent for determining whether two tables
contain overlapping IP addresses.

## Example

```golang
func ExampleTable_Contains() {
	// Create a new routing table
	table := new(bart.Table[struct{}])

	// Insert some prefixes
	prefixes := []string{
		"192.168.0.0/16",       // corporate
		"192.168.1.0/24",       // department
		"2001:7c0:3100::/40",   // corporate
		"2001:7c0:3100:1::/64", // department
		"fc00::/7",             // unique local
	}

	for _, s := range prefixes {
		pfx := netip.MustParsePrefix(s)
		table.Insert(pfx, struct{}{})
	}

	// Test some IP addresses for black/whitelist containment
	ips := []string{
		"192.168.1.100",      // must match, department
		"192.168.2.1",        // must match, corporate
		"2001:7c0:3100:1::1", // must match, department
		"2001:7c0:3100:2::1", // must match, corporate
		"fc00::1",            // must match, unique local
		//
		"172.16.0.1",        // must NOT match
		"2003:dead:beef::1", // must NOT match
	}

	for _, s := range ips {
		ip := netip.MustParseAddr(s)
		fmt.Printf("%-20s is contained: %t\n", ip, table.Contains(ip))
	}

	// Output:
	// 192.168.1.100        is contained: true
	// 192.168.2.1          is contained: true
	// 2001:7c0:3100:1::1   is contained: true
	// 2001:7c0:3100:2::1   is contained: true
	// fc00::1              is contained: true
	// 172.16.0.1           is contained: false
	// 2003:dead:beef::1    is contained: false
}
```
## API

Release v0.18 requires at least go1.23 and we use the `iter.Seq2[netip.Prefix, V]` types for iterators.
The lock-free versions of insert, update and delete are added, but still experimentell.

```golang
  import "github.com/gaissmai/bart"
  
  type Table[V any] struct {
  	// Has unexported fields.
  }
    Table is an IPv4 and IPv6 routing table with payload V. The zero value is
    ready to use.

    The Table is safe for concurrent readers but not for concurrent readers
    and/or writers. Either the update operations must be protected by an
    external lock mechanism or the various ...Persist functions must be used
    which return a modified routing table by leaving the original unchanged

    A Table must not be copied by value, see Table.Clone.

  func (t *Table[V]) Insert(pfx netip.Prefix, val V)
  func (t *Table[V]) Delete(pfx netip.Prefix)
  func (t *Table[V]) Update(pfx netip.Prefix, cb func(val V, ok bool) V) (newVal V)

  func (t *Table[V]) InsertPersist(pfx netip.Prefix, val V) *Table[V]
  func (t *Table[V]) DeletePersist(pfx netip.Prefix) *Table[V]
  func (t *Table[V]) UpdatePersist(pfx netip.Prefix, cb func(val V, ok bool) V) (pt *Table[V], newVal V)

  func (t *Table[V]) Get(pfx netip.Prefix) (val V, ok bool)
  func (t *Table[V]) GetAndDelete(pfx netip.Prefix) (val V, ok bool)
  func (t *Table[V]) GetAndDeletePersist(pfx netip.Prefix) (pt *Table[V], val V, ok bool)

  func (t *Table[V]) Union(o *Table[V])
  func (t *Table[V]) Clone() *Table[V]

  func (t *Table[V]) Contains(ip netip.Addr) bool
  func (t *Table[V]) Lookup(ip netip.Addr) (val V, ok bool)
  func (t *Table[V]) LookupPrefix(pfx netip.Prefix) (val V, ok bool)
  func (t *Table[V]) LookupPrefixLPM(pfx netip.Prefix) (lpm netip.Prefix, val V, ok bool)

  func (t *Table[V]) OverlapsPrefix(pfx netip.Prefix) bool

  func (t *Table[V]) Overlaps(o *Table[V])  bool
  func (t *Table[V]) Overlaps4(o *Table[V]) bool
  func (t *Table[V]) Overlaps6(o *Table[V]) bool

  func (t *Table[V]) Subnets(pfx netip.Prefix) iter.Seq2[netip.Prefix, V]
  func (t *Table[V]) Supernets(pfx netip.Prefix) iter.Seq2[netip.Prefix, V]

  func (t *Table[V]) All() iter.Seq2[netip.Prefix, V]
  func (t *Table[V]) All4() iter.Seq2[netip.Prefix, V]
  func (t *Table[V]) All6() iter.Seq2[netip.Prefix, V]

  func (t *Table[V]) AllSorted() iter.Seq2[netip.Prefix, V]
  func (t *Table[V]) AllSorted4() iter.Seq2[netip.Prefix, V]
  func (t *Table[V]) AllSorted6() iter.Seq2[netip.Prefix, V]

  func (t *Table[V]) Size() int
  func (t *Table[V]) Size4() int
  func (t *Table[V]) Size6() int

  func (t *Table[V]) String() string
  func (t *Table[V]) Fprint(w io.Writer) error
  func (t *Table[V]) MarshalText() ([]byte, error)
  func (t *Table[V]) MarshalJSON() ([]byte, error)

  func (t *Table[V]) DumpList4() []DumpListNode[V]
  func (t *Table[V]) DumpList6() []DumpListNode[V]
```

## benchmarks

Please see the extensive [benchmarks](https://github.com/gaissmai/iprbench) comparing `bart` with other IP routing table implementations.

Just a teaser, Contains and Lookups against the full Internet routing table with random IP address probes:

```
goos: linux
goarch: amd64
pkg: github.com/gaissmai/bart
cpu: Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz
BenchmarkFullMatchV4/Contains           55906228        21.39  ns/op       0 B/op       0 allocs/op
BenchmarkFullMatchV6/Contains           100000000       11.38  ns/op       0 B/op       0 allocs/op
BenchmarkFullMissV4/Contains            52927538        22.69  ns/op       0 B/op       0 allocs/op
BenchmarkFullMissV6/Contains            250386540        4.883 ns/op       0 B/op       0 allocs/op
PASS
ok      github.com/gaissmai/bart    11.291s

goos: linux
goarch: amd64
pkg: github.com/gaissmai/bart
cpu: Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz
BenchmarkFullMatchV4/Lookup             53578766        21.77  ns/op       0 B/op       0 allocs/op
BenchmarkFullMatchV6/Lookup             57054618        21.07  ns/op       0 B/op       0 allocs/op
BenchmarkFullMissV4/Lookup              48289306        25.60  ns/op       0 B/op       0 allocs/op
BenchmarkFullMissV6/Lookup              142917571        8.387 ns/op       0 B/op       0 allocs/op
PASS
ok      github.com/gaissmai/bart    10.455s
```

## Compatibility Guarantees

The package is currently released as a pre-v1 version, which gives the author the freedom to break
backward compatibility to help improve the API as he learns which initial design decisions would need
to be revisited to better support the use cases that the library solves for.

These occurrences are expected to be rare in frequency and the API is already quite stable.

## CONTRIBUTION

Please open an issue for discussion before sending a pull request.

## CREDIT

Standing on the shoulders of giants.

Credits for many inspirations go to

- the clever guys at tailscale,
- to Daniel Lemire for his inspiring blog,
- to Donald E. Knuth for the **ART** routing algorithm and
- to Yoichi Hariguchi who deciphered it for us mere mortals

And last but not least to the Go team who do a wonderful job!

## LICENSE

MIT
