// Package guts provides a low-level interface to the BLAKE3 cryptographic hash
// function.
package guts

// Various constants.
const (
	FlagChunkStart = 1 << iota
	FlagChunkEnd
	FlagParent
	FlagRoot
	FlagKeyedHash
	FlagDeriveKeyContext
	FlagDeriveKeyMaterial

	BlockSize = 64
	ChunkSize = 1024

	MaxSIMD = 16 // AVX-512 vectors can store 16 words
)

// IV is the BLAKE3 initialization vector.
var IV = [8]uint32{
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
}

// A Node represents a chunk or parent in the BLAKE3 Merkle tree.
type Node struct {
	CV       [8]uint32 // chaining value from previous node
	Block    [16]uint32
	Counter  uint64
	BlockLen uint32
	Flags    uint32
}

// ParentNode returns a Node that incorporates the chaining values of two child
// nodes.
func ParentNode(left, right [8]uint32, key *[8]uint32, flags uint32) Node {
	n := Node{
		CV:       *key,
		Counter:  0,         // counter is reset for parents
		BlockLen: BlockSize, // block is full
		Flags:    flags | FlagParent,
	}
	copy(n.Block[:8], left[:])
	copy(n.Block[8:], right[:])
	return n
}
