//go:build !amd64
// +build !amd64

package guts

import "encoding/binary"

// CompressBuffer compresses up to MaxSIMD chunks in parallel and returns their
// root node.
func CompressBuffer(buf *[MaxSIMD * ChunkSize]byte, buflen int, key *[8]uint32, counter uint64, flags uint32) Node {
	return compressBufferGeneric(buf, buflen, key, counter, flags)
}

// CompressChunk compresses a single chunk, returning its final (uncompressed)
// node.
func CompressChunk(chunk []byte, key *[8]uint32, counter uint64, flags uint32) Node {
	n := Node{
		CV:       *key,
		Counter:  counter,
		BlockLen: BlockSize,
		Flags:    flags | FlagChunkStart,
	}
	var block [BlockSize]byte
	for len(chunk) > BlockSize {
		copy(block[:], chunk)
		chunk = chunk[BlockSize:]
		n.Block = BytesToWords(block)
		n.CV = ChainingValue(n)
		n.Flags &^= FlagChunkStart
	}
	// pad last block with zeros
	block = [BlockSize]byte{}
	n.BlockLen = uint32(copy(block[:], chunk))
	n.Block = BytesToWords(block)
	n.Flags |= FlagChunkEnd
	return n
}

// CompressBlocks compresses MaxSIMD copies of n with successive counter values,
// storing the results in out.
func CompressBlocks(out *[MaxSIMD * BlockSize]byte, n Node) {
	var outs [MaxSIMD][64]byte
	compressBlocksGeneric(&outs, n)
	for i := range outs {
		copy(out[i*64:], outs[i][:])
	}
}

func mergeSubtrees(cvs *[MaxSIMD][8]uint32, numCVs uint64, key *[8]uint32, flags uint32) Node {
	return mergeSubtreesGeneric(cvs, numCVs, key, flags)
}

// BytesToWords converts an array of 64 bytes to an array of 16 bytes.
func BytesToWords(bytes [64]byte) (words [16]uint32) {
	for i := range words {
		words[i] = binary.LittleEndian.Uint32(bytes[4*i:])
	}
	return
}

// WordsToBytes converts an array of 16 words to an array of 64 bytes.
func WordsToBytes(words [16]uint32) (block [64]byte) {
	for i, w := range words {
		binary.LittleEndian.PutUint32(block[4*i:], w)
	}
	return
}
