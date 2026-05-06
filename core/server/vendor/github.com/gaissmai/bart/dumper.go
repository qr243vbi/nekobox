// Copyright (c) 2024 Karl Gaissmaier
// SPDX-License-Identifier: MIT

package bart

import (
	"fmt"
	"io"
	"strconv"
	"strings"
)

type nodeType byte

const (
	nullNode         nodeType = iota // empty node
	fullNode                         // prefixes and children or path-compressed prefixes
	leafNode                         // no children, only prefixes or path-compressed prefixes
	intermediateNode                 // only children, no prefix nor path-compressed prefixes
)

// ##################################################
//  useful during development, debugging and testing
// ##################################################

// dumpString is just a wrapper for dump.
func (t *Table[V]) dumpString() string {
	w := new(strings.Builder)
	t.dump(w)

	return w.String()
}

// dump the table structure and all the nodes to w.
func (t *Table[V]) dump(w io.Writer) {
	if t == nil {
		return
	}

	if t.size4 > 0 {
		fmt.Fprintln(w)
		fmt.Fprintf(w, "### IPv4: size(%d), nodes(%d)", t.size4, t.root4.nodeStatsRec().nodes)
		t.root4.dumpRec(w, stridePath{}, 0, true)
	}

	if t.size6 > 0 {
		fmt.Fprintln(w)
		fmt.Fprintf(w, "### IPv6: size(%d), nodes(%d)", t.size6, t.root6.nodeStatsRec().nodes)
		t.root6.dumpRec(w, stridePath{}, 0, false)
	}
}

// dumpRec, rec-descent the trie.
func (n *node[V]) dumpRec(w io.Writer, path stridePath, depth int, is4 bool) {
	// dump this node
	n.dump(w, path, depth, is4)

	// the node may have childs, rec-descent down
	for i, addr := range n.children.All() {
		octet := byte(addr)
		path[depth] = octet

		if child, ok := n.children.Items[i].(*node[V]); ok {
			child.dumpRec(w, path, depth+1, is4)
		}
	}
}

// dump the node to w.
func (n *node[V]) dump(w io.Writer, path stridePath, depth int, is4 bool) {
	bits := depth * strideLen
	indent := strings.Repeat(".", depth)

	// node type with depth and octet path and bits.
	fmt.Fprintf(w, "\n%s[%s] depth:  %d path: [%s] / %d\n",
		indent, n.hasType(), depth, ipStridePath(path, depth, is4), bits)

	if nPfxCount := n.prefixes.Len(); nPfxCount != 0 {
		// no heap allocs
		allIndices := n.prefixes.All()

		// print the baseIndices for this node.
		fmt.Fprintf(w, "%sindexs(#%d): %v\n", indent, nPfxCount, allIndices)

		// print the prefixes for this node
		fmt.Fprintf(w, "%sprefxs(#%d):", indent, nPfxCount)

		for _, idx := range allIndices {
			octet, pfxLen := idxToPfx(idx)
			fmt.Fprintf(w, " %s/%d", octetFmt(octet, is4), pfxLen)
		}

		fmt.Fprintln(w)

		// print the values for this node
		fmt.Fprintf(w, "%svalues(#%d):", indent, nPfxCount)

		for _, val := range n.prefixes.Items {
			fmt.Fprintf(w, " %v", val)
		}

		fmt.Fprintln(w)
	}

	if n.children.Len() != 0 {

		nodeAddrs := make([]uint, 0, maxNodeChildren)
		leafAddrs := make([]uint, 0, maxNodeChildren)

		// the node has recursive child nodes or path-compressed leaves
		for i, addr := range n.children.All() {
			switch n.children.Items[i].(type) {
			case *node[V]:
				nodeAddrs = append(nodeAddrs, addr)
				continue

			case *leaf[V]:
				leafAddrs = append(leafAddrs, addr)

			default:
				panic("logic error, wrong node type")
			}
		}

		if nodeCount := len(nodeAddrs); nodeCount > 0 {
			// print the childs for this node
			fmt.Fprintf(w, "%schilds(#%d):", indent, nodeCount)

			for _, addr := range nodeAddrs {
				octet := byte(addr)
				fmt.Fprintf(w, " %s", octetFmt(octet, is4))
			}

			fmt.Fprintln(w)
		}

		if leafCount := len(leafAddrs); leafCount > 0 {
			// print the pathcomp prefixes for this node
			fmt.Fprintf(w, "%sleaves(#%d):", indent, leafCount)

			for _, addr := range leafAddrs {
				octet := byte(addr)
				k := n.children.MustGet(addr)
				pc := k.(*leaf[V])

				fmt.Fprintf(w, " %s:{%s, %v}", octetFmt(octet, is4), pc.prefix, pc.value)
			}
			fmt.Fprintln(w)
		}
	}
}

// hasType returns the nodeType.
func (n *node[V]) hasType() nodeType {
	s := n.nodeStats()

	switch {
	case s.pfxs == 0 && s.childs == 0:
		return nullNode
	case s.nodes == 0:
		return leafNode
	case (s.pfxs > 0 || s.leaves > 0) && s.nodes > 0:
		return fullNode
	case (s.pfxs == 0 && s.leaves == 0) && s.nodes > 0:
		return intermediateNode
	default:
		panic(fmt.Sprintf("UNREACHABLE: pfx: %d, chld: %d, node: %d, leaf: %d",
			s.pfxs, s.childs, s.nodes, s.leaves))
	}
}

// octetFmt, different format strings for IPv4 and IPv6, decimal versus hex.
func octetFmt(octet byte, is4 bool) string {
	if is4 {
		return fmt.Sprintf("%d", octet)
	}

	return fmt.Sprintf("0x%02x", octet)
}

// ip stride path, different formats for IPv4 and IPv6, dotted decimal or hex.
//
//	127.0.0
//	2001:0d
func ipStridePath(path stridePath, depth int, is4 bool) string {
	buf := new(strings.Builder)

	if is4 {
		for i, b := range path[:depth] {
			if i != 0 {
				buf.WriteString(".")
			}

			buf.WriteString(strconv.Itoa(int(b)))
		}

		return buf.String()
	}

	for i, b := range path[:depth] {
		if i != 0 && i%2 == 0 {
			buf.WriteString(":")
		}

		buf.WriteString(fmt.Sprintf("%02x", b))
	}

	return buf.String()
}

// String implements Stringer for nodeType.
func (nt nodeType) String() string {
	switch nt {
	case nullNode:
		return "NULL"
	case fullNode:
		return "FULL"
	case leafNode:
		return "LEAF"
	case intermediateNode:
		return "IMED"
	default:
		return "unreachable"
	}
}

// stats, only used for dump, tests and benchmarks
type stats struct {
	pfxs   int
	childs int
	nodes  int
	leaves int
}

// node statistics for this single node
func (n *node[V]) nodeStats() stats {
	var s stats

	s.pfxs = n.prefixes.Len()
	s.childs = n.children.Len()

	for i := range n.children.All() {
		switch n.children.Items[i].(type) {
		case *node[V]:
			s.nodes++

		case *leaf[V]:
			s.leaves++

		default:
			panic("logic error, wrong node type")
		}
	}

	return s
}

// nodeStatsRec, calculate the number of pfxs, nodes and leaves under n, rec-descent.
func (n *node[V]) nodeStatsRec() stats {
	var s stats
	if n == nil || n.isEmpty() {
		return s
	}

	s.pfxs = n.prefixes.Len()
	s.childs = n.children.Len()
	s.nodes = 1 // this node
	s.leaves = 0

	for _, kidAny := range n.children.Items {
		switch kid := kidAny.(type) {
		case *node[V]:
			// rec-descent
			rs := kid.nodeStatsRec()

			s.pfxs += rs.pfxs
			s.childs += rs.childs
			s.nodes += rs.nodes
			s.leaves += rs.leaves

		case *leaf[V]:
			s.leaves++

		default:
			panic("logic error, wrong node type")
		}
	}

	return s
}
