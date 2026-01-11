package guts

import "unsafe"

//go:generate go run avo/gen.go -out blake3_amd64.s

//go:noescape
func compressChunksAVX512(cvs *[16][8]uint32, buf *[16 * ChunkSize]byte, key *[8]uint32, counter uint64, flags uint32)

//go:noescape
func compressChunksAVX2(cvs *[8][8]uint32, buf *[8 * ChunkSize]byte, key *[8]uint32, counter uint64, flags uint32)

//go:noescape
func compressBlocksAVX512(out *[1024]byte, block *[16]uint32, cv *[8]uint32, counter uint64, blockLen uint32, flags uint32)

//go:noescape
func compressBlocksAVX2(out *[512]byte, msgs *[16]uint32, cv *[8]uint32, counter uint64, blockLen uint32, flags uint32)

//go:noescape
func compressParentsAVX2(parents *[8][8]uint32, cvs *[16][8]uint32, key *[8]uint32, flags uint32)

func compressBufferAVX512(buf *[MaxSIMD * ChunkSize]byte, buflen int, key *[8]uint32, counter uint64, flags uint32) Node {
	var cvs [MaxSIMD][8]uint32
	compressChunksAVX512(&cvs, buf, key, counter, flags)
	numChunks := uint64(buflen / ChunkSize)
	if buflen%ChunkSize != 0 {
		// use non-asm for remainder
		partialChunk := buf[buflen-buflen%ChunkSize : buflen]
		cvs[numChunks] = ChainingValue(CompressChunk(partialChunk, key, counter+numChunks, flags))
		numChunks++
	}
	return mergeSubtrees(&cvs, numChunks, key, flags)
}

func compressBufferAVX2(buf *[MaxSIMD * ChunkSize]byte, buflen int, key *[8]uint32, counter uint64, flags uint32) Node {
	var cvs [MaxSIMD][8]uint32
	cvHalves := (*[2][8][8]uint32)(unsafe.Pointer(&cvs))
	bufHalves := (*[2][8 * ChunkSize]byte)(unsafe.Pointer(buf))
	compressChunksAVX2(&cvHalves[0], &bufHalves[0], key, counter, flags)
	numChunks := uint64(buflen / ChunkSize)
	if numChunks > 8 {
		compressChunksAVX2(&cvHalves[1], &bufHalves[1], key, counter+8, flags)
	}
	if buflen%ChunkSize != 0 {
		// use non-asm for remainder
		partialChunk := buf[buflen-buflen%ChunkSize : buflen]
		cvs[numChunks] = ChainingValue(CompressChunk(partialChunk, key, counter+numChunks, flags))
		numChunks++
	}
	return mergeSubtrees(&cvs, numChunks, key, flags)
}

// CompressBuffer compresses up to MaxSIMD chunks in parallel and returns their
// root node.
func CompressBuffer(buf *[MaxSIMD * ChunkSize]byte, buflen int, key *[8]uint32, counter uint64, flags uint32) Node {
	if buflen <= ChunkSize {
		return CompressChunk(buf[:buflen], key, counter, flags)
	}
	switch {
	case haveAVX512 && buflen >= ChunkSize*2:
		return compressBufferAVX512(buf, buflen, key, counter, flags)
	case haveAVX2 && buflen >= ChunkSize*2:
		return compressBufferAVX2(buf, buflen, key, counter, flags)
	default:
		return compressBufferGeneric(buf, buflen, key, counter, flags)
	}
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
	blockBytes := (*[64]byte)(unsafe.Pointer(&n.Block))[:]
	for len(chunk) > BlockSize {
		copy(blockBytes, chunk)
		chunk = chunk[BlockSize:]
		n.CV = ChainingValue(n)
		n.Flags &^= FlagChunkStart
	}
	// pad last block with zeros
	n.Block = [16]uint32{}
	copy(blockBytes, chunk)
	n.BlockLen = uint32(len(chunk))
	n.Flags |= FlagChunkEnd
	return n
}

// CompressBlocks compresses MaxSIMD copies of n with successive counter values,
// storing the results in out.
func CompressBlocks(out *[MaxSIMD * BlockSize]byte, n Node) {
	switch {
	case haveAVX512:
		compressBlocksAVX512(out, &n.Block, &n.CV, n.Counter, n.BlockLen, n.Flags)
	case haveAVX2:
		outs := (*[2][512]byte)(unsafe.Pointer(out))
		compressBlocksAVX2(&outs[0], &n.Block, &n.CV, n.Counter, n.BlockLen, n.Flags)
		compressBlocksAVX2(&outs[1], &n.Block, &n.CV, n.Counter+8, n.BlockLen, n.Flags)
	default:
		outs := (*[MaxSIMD][64]byte)(unsafe.Pointer(out))
		compressBlocksGeneric(outs, n)
	}
}

func mergeSubtrees(cvs *[MaxSIMD][8]uint32, numCVs uint64, key *[8]uint32, flags uint32) Node {
	if !haveAVX2 {
		return mergeSubtreesGeneric(cvs, numCVs, key, flags)
	}
	for numCVs > 2 {
		if numCVs%2 == 0 {
			compressParentsAVX2((*[8][8]uint32)(unsafe.Pointer(cvs)), cvs, key, flags)
		} else {
			keep := cvs[numCVs-1]
			compressParentsAVX2((*[8][8]uint32)(unsafe.Pointer(cvs)), cvs, key, flags)
			cvs[numCVs/2] = keep
			numCVs++
		}
		numCVs /= 2
	}
	return ParentNode(cvs[0], cvs[1], key, flags)
}

// BytesToWords converts an array of 64 bytes to an array of 16 bytes.
func BytesToWords(bytes [64]byte) [16]uint32 {
	return *(*[16]uint32)(unsafe.Pointer(&bytes))
}

// WordsToBytes converts an array of 16 words to an array of 64 bytes.
func WordsToBytes(words [16]uint32) [64]byte {
	return *(*[64]byte)(unsafe.Pointer(&words))
}
