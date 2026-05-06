// Package bao implements BLAKE3 verified streaming.
package bao

import (
	"bytes"
	"encoding/binary"
	"errors"
	"io"
	"math/bits"

	"lukechampine.com/blake3/guts"
)

func bytesToCV(b []byte) (cv [8]uint32) {
	_ = b[31] // bounds check hint
	for i := range cv {
		cv[i] = binary.LittleEndian.Uint32(b[4*i:])
	}
	return cv
}

func cvToBytes(cv *[8]uint32) *[32]byte {
	var b [32]byte
	for i, w := range cv {
		binary.LittleEndian.PutUint32(b[4*i:], w)
	}
	return &b
}

func compressGroup(p []byte, counter uint64) guts.Node {
	var stack [54 - guts.MaxSIMD][8]uint32
	var sc uint64
	pushSubtree := func(cv [8]uint32) {
		i := 0
		for sc&(1<<i) != 0 {
			cv = guts.ChainingValue(guts.ParentNode(stack[i], cv, &guts.IV, 0))
			i++
		}
		stack[i] = cv
		sc++
	}

	var buf [guts.MaxSIMD * guts.ChunkSize]byte
	var buflen int
	for len(p) > 0 {
		if buflen == len(buf) {
			pushSubtree(guts.ChainingValue(guts.CompressBuffer(&buf, buflen, &guts.IV, counter+(sc*guts.MaxSIMD), 0)))
			buflen = 0
		}
		n := copy(buf[buflen:], p)
		buflen += n
		p = p[n:]
	}
	n := guts.CompressBuffer(&buf, buflen, &guts.IV, counter+(sc*guts.MaxSIMD), 0)
	for i := bits.TrailingZeros64(sc); i < bits.Len64(sc); i++ {
		if sc&(1<<i) != 0 {
			n = guts.ParentNode(stack[i], guts.ChainingValue(n), &guts.IV, 0)
		}
	}
	return n
}

// EncodedSize returns the size of a Bao encoding for the provided quantity
// of data.
func EncodedSize(dataLen int, group int, outboard bool) int {
	groupSize := guts.ChunkSize << group
	size := 8
	if dataLen > 0 {
		chunks := (dataLen + groupSize - 1) / groupSize
		cvs := 2*chunks - 2 // no I will not elaborate
		size += cvs * 32
	}
	if !outboard {
		size += dataLen
	}
	return size
}

// Encode computes the intermediate BLAKE3 tree hashes of data and writes them
// to dst. If outboard is false, the contents of data are also written to dst,
// interleaved with the tree hashes. It also returns the tree root, i.e. the
// 256-bit BLAKE3 hash. The group parameter controls how many chunks are hashed
// per "group," as a power of 2; for standard Bao, use 0.
//
// Note that dst is not written sequentially, and therefore must be initialized
// with sufficient capacity to hold the encoding; see EncodedSize.
func Encode(dst io.WriterAt, data io.Reader, dataLen int64, group int, outboard bool) ([32]byte, error) {
	groupSize := uint64(guts.ChunkSize << group)
	buf := make([]byte, groupSize)
	var err error
	read := func(p []byte) []byte {
		if err == nil {
			_, err = io.ReadFull(data, p)
		}
		return p
	}
	write := func(p []byte, off uint64) {
		if err == nil {
			_, err = dst.WriteAt(p, int64(off))
		}
	}
	var counter uint64

	// NOTE: unlike the reference implementation, we write directly in
	// pre-order, rather than writing in post-order and then flipping. This cuts
	// the I/O required in half, at the cost of making it a lot trickier to hash
	// multiple groups in SIMD. However, you can still get the SIMD speedup if
	// group > 0, so maybe just do that.
	var rec func(bufLen uint64, flags uint32, off uint64) (uint64, [8]uint32)
	rec = func(bufLen uint64, flags uint32, off uint64) (uint64, [8]uint32) {
		if err != nil {
			return 0, [8]uint32{}
		} else if bufLen <= groupSize {
			g := read(buf[:bufLen])
			if !outboard {
				write(g, off)
			}
			n := compressGroup(g, counter)
			counter += bufLen / guts.ChunkSize
			n.Flags |= flags
			return 0, guts.ChainingValue(n)
		}
		mid := uint64(1) << (bits.Len64(bufLen-1) - 1)
		lchildren, l := rec(mid, 0, off+64)
		llen := lchildren * 32
		if !outboard {
			llen += (mid / groupSize) * groupSize
		}
		rchildren, r := rec(bufLen-mid, 0, off+64+llen)
		write(cvToBytes(&l)[:], off)
		write(cvToBytes(&r)[:], off+32)
		return 2 + lchildren + rchildren, guts.ChainingValue(guts.ParentNode(l, r, &guts.IV, flags))
	}

	binary.LittleEndian.PutUint64(buf[:8], uint64(dataLen))
	write(buf[:8], 0)
	_, root := rec(uint64(dataLen), guts.FlagRoot, 8)
	return *cvToBytes(&root), err
}

// Decode reads content and tree data from the provided reader(s), and
// streams the verified content to dst. It returns false if verification fails.
// If the content and tree data are interleaved, outboard should be nil.
func Decode(dst io.Writer, data, outboard io.Reader, group int, root [32]byte) (bool, error) {
	if outboard == nil {
		outboard = data
	}
	groupSize := uint64(guts.ChunkSize << group)
	buf := make([]byte, groupSize)
	var err error
	read := func(r io.Reader, p []byte) []byte {
		if err == nil {
			_, err = io.ReadFull(r, p)
		}
		return p
	}
	write := func(w io.Writer, p []byte) {
		if err == nil {
			_, err = w.Write(p)
		}
	}
	readParent := func() (l, r [8]uint32) {
		read(outboard, buf[:64])
		return bytesToCV(buf[:32]), bytesToCV(buf[32:])
	}
	var counter uint64
	var rec func(cv [8]uint32, bufLen uint64, flags uint32) bool
	rec = func(cv [8]uint32, bufLen uint64, flags uint32) bool {
		if err != nil {
			return false
		} else if bufLen <= groupSize {
			n := compressGroup(read(data, buf[:bufLen]), counter)
			counter += bufLen / guts.ChunkSize
			n.Flags |= flags
			valid := cv == guts.ChainingValue(n)
			if valid {
				write(dst, buf[:bufLen])
			}
			return valid
		}
		l, r := readParent()
		n := guts.ParentNode(l, r, &guts.IV, flags)
		mid := uint64(1) << (bits.Len64(bufLen-1) - 1)
		return guts.ChainingValue(n) == cv && rec(l, mid, 0) && rec(r, bufLen-mid, 0)
	}

	read(outboard, buf[:8])
	dataLen := binary.LittleEndian.Uint64(buf[:8])
	ok := rec(bytesToCV(root[:]), dataLen, guts.FlagRoot)
	return ok, err
}

type bufferAt struct {
	buf []byte
}

func (b *bufferAt) WriteAt(p []byte, off int64) (int, error) {
	if copy(b.buf[off:], p) != len(p) {
		panic("bad buffer size")
	}
	return len(p), nil
}

// EncodeBuf returns the Bao encoding and root (i.e. BLAKE3 hash) for data.
func EncodeBuf(data []byte, group int, outboard bool) ([]byte, [32]byte) {
	buf := bufferAt{buf: make([]byte, EncodedSize(len(data), group, outboard))}
	root, _ := Encode(&buf, bytes.NewReader(data), int64(len(data)), group, outboard)
	return buf.buf, root
}

// VerifyBuf verifies the Bao encoding and root (i.e. BLAKE3 hash) for data.
// If the content and tree data are interleaved, outboard should be nil.
func VerifyBuf(data, outboard []byte, group int, root [32]byte) bool {
	d, o := bytes.NewBuffer(data), bytes.NewBuffer(outboard)
	var or io.Reader = o
	if outboard == nil {
		or = nil
	}
	ok, _ := Decode(io.Discard, d, or, group, root)
	return ok && d.Len() == 0 && o.Len() == 0 // check for trailing data
}

// ExtractSlice returns the slice encoding for the given offset and length. When
// extracting from an outboard encoding, data should contain only the chunk
// groups that will be present in the slice.
func ExtractSlice(dst io.Writer, data, outboard io.Reader, group int, offset uint64, length uint64) error {
	combinedEncoding := outboard == nil
	if combinedEncoding {
		outboard = data
	}
	groupSize := uint64(guts.ChunkSize << group)
	buf := make([]byte, groupSize)
	var err error
	read := func(r io.Reader, n uint64, copy bool) {
		if err == nil {
			_, err = io.ReadFull(r, buf[:n])
			if err == nil && copy {
				_, err = dst.Write(buf[:n])
			}
		}
	}
	var rec func(pos, bufLen uint64)
	rec = func(pos, bufLen uint64) {
		inSlice := pos < (offset+length) && offset < (pos+bufLen)
		if err != nil {
			return
		} else if bufLen <= groupSize {
			if combinedEncoding || inSlice {
				read(data, bufLen, inSlice)
			}
			return
		}
		read(outboard, 64, inSlice)
		mid := uint64(1) << (bits.Len64(bufLen-1) - 1)
		rec(pos, mid)
		rec(pos+mid, bufLen-mid)
	}
	read(outboard, 8, true)
	dataLen := binary.LittleEndian.Uint64(buf[:8])
	if dataLen < offset+length {
		return errors.New("invalid slice length")
	}
	rec(0, dataLen)
	return err
}

// DecodeSlice reads from data, which must contain a slice encoding for the
// given offset and length, and streams verified content to dst. It returns
// false if verification fails.
func DecodeSlice(dst io.Writer, data io.Reader, group int, offset, length uint64, root [32]byte) (bool, error) {
	groupSize := uint64(guts.ChunkSize << group)
	buf := make([]byte, groupSize)
	var err error
	read := func(n uint64) []byte {
		if err == nil {
			_, err = io.ReadFull(data, buf[:n])
		}
		return buf[:n]
	}
	readParent := func() (l, r [8]uint32) {
		read(64)
		return bytesToCV(buf[:32]), bytesToCV(buf[32:])
	}
	write := func(p []byte) {
		if err == nil {
			_, err = dst.Write(p)
		}
	}
	var rec func(cv [8]uint32, pos, bufLen uint64, flags uint32) bool
	rec = func(cv [8]uint32, pos, bufLen uint64, flags uint32) bool {
		inSlice := pos < (offset+length) && offset < (pos+bufLen)
		if err != nil {
			return false
		} else if bufLen <= groupSize {
			if !inSlice {
				return true
			}
			n := compressGroup(read(bufLen), pos/guts.ChunkSize)
			n.Flags |= flags
			valid := cv == guts.ChainingValue(n)
			if valid {
				// only write within range
				p := buf[:bufLen]
				if pos+bufLen > offset+length {
					p = p[:offset+length-pos]
				}
				if pos < offset {
					p = p[offset-pos:]
				}
				write(p)
			}
			return valid
		}
		if !inSlice {
			return true
		}
		l, r := readParent()
		n := guts.ParentNode(l, r, &guts.IV, flags)
		mid := uint64(1) << (bits.Len64(bufLen-1) - 1)
		return guts.ChainingValue(n) == cv && rec(l, pos, mid, 0) && rec(r, pos+mid, bufLen-mid, 0)
	}

	dataLen := binary.LittleEndian.Uint64(read(8))
	if dataLen < offset+length {
		return false, errors.New("invalid slice length")
	}
	ok := rec(bytesToCV(root[:]), 0, dataLen, guts.FlagRoot)
	return ok, err
}

// VerifySlice verifies the Bao slice encoding in data, returning the
// verified bytes.
func VerifySlice(data []byte, group int, offset uint64, length uint64, root [32]byte) ([]byte, bool) {
	d := bytes.NewBuffer(data)
	var buf bytes.Buffer
	if ok, _ := DecodeSlice(&buf, d, group, offset, length, root); !ok || d.Len() > 0 {
		return nil, false
	}
	return buf.Bytes(), true
}

// VerifyChunks verifies the provided chunks with a full outboard encoding.
func VerifyChunk(chunks, outboard []byte, group int, offset uint64, root [32]byte) bool {
	cbuf := bytes.NewBuffer(chunks)
	obuf := bytes.NewBuffer(outboard)
	groupSize := uint64(guts.ChunkSize << group)
	length := uint64(len(chunks))
	nodesWithin := func(bufLen uint64) int {
		n := int(bufLen / groupSize)
		if bufLen%groupSize == 0 {
			n--
		}
		return n
	}

	var rec func(cv [8]uint32, pos, bufLen uint64, flags uint32) bool
	rec = func(cv [8]uint32, pos, bufLen uint64, flags uint32) bool {
		inSlice := pos < (offset+length) && offset < (pos+bufLen)
		if bufLen <= groupSize {
			if !inSlice {
				return true
			}
			n := compressGroup(cbuf.Next(int(groupSize)), pos/guts.ChunkSize)
			n.Flags |= flags
			return cv == guts.ChainingValue(n)
		}
		if !inSlice {
			_ = obuf.Next(64 * nodesWithin(bufLen)) // skip
			return true
		}
		l, r := bytesToCV(obuf.Next(32)), bytesToCV(obuf.Next(32))
		n := guts.ParentNode(l, r, &guts.IV, flags)
		mid := uint64(1) << (bits.Len64(bufLen-1) - 1)
		return guts.ChainingValue(n) == cv && rec(l, pos, mid, 0) && rec(r, pos+mid, bufLen-mid, 0)
	}

	if obuf.Len() < 8 {
		return false
	}
	dataLen := binary.LittleEndian.Uint64(obuf.Next(8))
	if dataLen < offset+length || obuf.Len() != 64*nodesWithin(dataLen) {
		return false
	}
	return rec(bytesToCV(root[:]), 0, dataLen, guts.FlagRoot)
}
