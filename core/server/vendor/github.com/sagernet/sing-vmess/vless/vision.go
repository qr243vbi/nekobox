package vless

import (
	"bytes"
	"crypto/rand"
	"crypto/tls"
	"io"
	"math/big"
	"net"
	"reflect"
	"sync"
	"unsafe"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	N "github.com/sagernet/sing/common/network"
)

var tlsRegistry []func(conn net.Conn) (loaded bool, netConn net.Conn, reflectType reflect.Type, reflectPointer uintptr)

func init() {
	tlsRegistry = append(tlsRegistry, func(conn net.Conn) (loaded bool, netConn net.Conn, reflectType reflect.Type, reflectPointer uintptr) {
		tlsConn, loaded := N.CastReader[*tls.Conn](conn)
		if !loaded {
			return
		}
		return true, tlsConn.NetConn(), reflect.TypeOf(tlsConn).Elem(), uintptr(unsafe.Pointer(tlsConn))
	})
}

const xrayChunkSize = 8192

type VisionConn struct {
	net.Conn
	reader      *bufio.ChunkReader
	writer      N.VectorisedWriter
	writeAccess sync.Mutex
	input       *bytes.Reader
	rawInput    *bytes.Buffer
	netConn     net.Conn
	rawConn     net.Conn // raw connection for Vision internal operations (TLS/encryption layer)
	directConn  net.Conn
	logger      logger.Logger

	userUUID               [16]byte
	isTLS                  bool
	numberOfPacketToFilter int
	isTLS12orAbove         bool
	remainingServerHello   int32
	cipher                 uint16
	enableXTLS             bool
	canSplice              bool
	isPadding              bool
	directWrite            bool
	writeUUID              bool
	withinPaddingBuffers   bool
	remainingContent       int
	remainingPadding       int
	currentCommand         byte
	directRead             bool
	remainingBuffers       []*buf.Buffer
}

type EncryptionConn interface {
	net.Conn
	IsEncryptionLayer() bool
}

type visionConnInfo struct {
	netConn  net.Conn
	rawConn  net.Conn
	input    *bytes.Reader
	rawInput *bytes.Buffer
}

func NewVisionConn(conn net.Conn, tlsConn net.Conn, userUUID [16]byte, logger logger.Logger, canSplice bool) (*VisionConn, error) {
	// tlsConn can be:
	// 1. TLS connection (when TLS/Reality is used)
	// 2. Encryption layer (when only encryption is used, no TLS/Reality)
	baseConn := unwrapConn(tlsConn)

	info, err := extractVisionConnInfo(conn, baseConn)
	if err != nil {
		return nil, err
	}

	return &VisionConn{
		Conn:       conn,
		reader:     bufio.NewChunkReader(conn, xrayChunkSize),
		writer:     bufio.NewVectorisedWriter(conn),
		input:      info.input,
		rawInput:   info.rawInput,
		netConn:    info.netConn,
		rawConn:    info.rawConn,
		directConn: info.rawConn,
		logger:     logger,

		userUUID:               userUUID,
		numberOfPacketToFilter: 8,
		remainingServerHello:   -1,
		canSplice:              canSplice,
		isPadding:              true,
		writeUUID:              true,
		withinPaddingBuffers:   true,
		remainingContent:       -1,
		remainingPadding:       -1,
	}, nil
}

func extractVisionConnInfo(conn net.Conn, baseConn net.Conn) (*visionConnInfo, error) {
	if isEncryptionConn(baseConn) {
		return extractEncryptionInfo(conn, baseConn)
	}

	encConn := findEncryptionInStack(conn)
	if encConn != nil {
		return extractTLSWithEncryptionInfo(baseConn, encConn)
	}

	return extractTLSInfo(baseConn)
}

func extractEncryptionInfo(conn net.Conn, encConn net.Conn) (*visionConnInfo, error) {
	info := &visionConnInfo{}

	if upstream, ok := encConn.(common.WithUpstream); ok {
		if upstreamConn, ok := upstream.Upstream().(net.Conn); ok {
			info.netConn = upstreamConn
			info.rawConn = upstreamConn
		}
	}

	if err := extractInputFields(encConn, info); err != nil {
		return nil, err
	}

	return info, nil
}

func extractTLSWithEncryptionInfo(tlsConn net.Conn, encConn net.Conn) (*visionConnInfo, error) {
	info := &visionConnInfo{}

	for _, tlsCreator := range tlsRegistry {
		loaded, netConn, _, _ := tlsCreator(tlsConn)
		if loaded {
			info.netConn = netConn
			break
		}
	}
	if info.netConn == nil {
		return nil, E.New("vision: not a valid supported TLS connection: ", reflect.TypeOf(tlsConn))
	}

	if isXorConn(encConn) {
		info.rawConn = encConn
		if err := extractInputFieldsFromTLS(tlsConn, info); err != nil {
			return nil, err
		}
	} else {
		if upstream, ok := encConn.(common.WithUpstream); ok {
			if upstreamConn, ok := upstream.Upstream().(net.Conn); ok {
				info.rawConn = upstreamConn
			}
		}
		if err := extractInputFields(encConn, info); err != nil {
			return nil, err
		}
	}

	return info, nil
}

func extractTLSInfo(tlsConn net.Conn) (*visionConnInfo, error) {
	info := &visionConnInfo{}

	for _, tlsCreator := range tlsRegistry {
		loaded, netConn, reflectType, reflectPointer := tlsCreator(tlsConn)
		if loaded {
			info.netConn = netConn
			info.rawConn = netConn

			inputField, _ := reflectType.FieldByName("input")
			rawInputField, _ := reflectType.FieldByName("rawInput")
			info.input = (*bytes.Reader)(unsafe.Pointer(reflectPointer + inputField.Offset))
			info.rawInput = (*bytes.Buffer)(unsafe.Pointer(reflectPointer + rawInputField.Offset))
			return info, nil
		}
	}

	return nil, E.New("vision: not a valid supported TLS connection: ", reflect.TypeOf(tlsConn))
}

func extractInputFields(encConn net.Conn, info *visionConnInfo) error {
	reflectType := reflect.TypeOf(encConn).Elem()
	reflectPointer := uintptr(unsafe.Pointer(reflect.ValueOf(encConn).Pointer()))

	inputField, inputOk := reflectType.FieldByName("input")
	rawInputField, rawInputOk := reflectType.FieldByName("rawInput")

	if !inputOk || !rawInputOk {
		return E.New("vision: encryption layer missing input/rawInput fields")
	}

	info.input = (*bytes.Reader)(unsafe.Pointer(reflectPointer + inputField.Offset))
	info.rawInput = (*bytes.Buffer)(unsafe.Pointer(reflectPointer + rawInputField.Offset))
	return nil
}

func extractInputFieldsFromTLS(tlsConn net.Conn, info *visionConnInfo) error {
	for _, tlsCreator := range tlsRegistry {
		loaded, _, reflectType, reflectPointer := tlsCreator(tlsConn)
		if loaded {
			inputField, _ := reflectType.FieldByName("input")
			rawInputField, _ := reflectType.FieldByName("rawInput")
			info.input = (*bytes.Reader)(unsafe.Pointer(reflectPointer + inputField.Offset))
			info.rawInput = (*bytes.Buffer)(unsafe.Pointer(reflectPointer + rawInputField.Offset))
			return nil
		}
	}
	return E.New("vision: failed to extract input fields from TLS connection")
}

func isEncryptionConn(conn net.Conn) bool {
	if enc, ok := conn.(EncryptionConn); ok {
		return enc.IsEncryptionLayer()
	}
	if reflect.TypeOf(conn).Kind() == reflect.Ptr {
		typeName := reflect.TypeOf(conn).Elem().Name()
		return typeName == "CommonConn" || typeName == "XorConn"
	}
	return false
}

func isXorConn(conn net.Conn) bool {
	if reflect.TypeOf(conn).Kind() == reflect.Ptr {
		return reflect.TypeOf(conn).Elem().Name() == "XorConn"
	}
	return false
}

func findEncryptionInStack(conn net.Conn) net.Conn {
	current := conn
	visited := make(map[uintptr]struct{})

	for current != nil {
		ptr := reflect.ValueOf(current).Pointer()
		if _, ok := visited[ptr]; ok {
			break
		}
		visited[ptr] = struct{}{}

		if isEncryptionConn(current) {
			return current
		}

		next := getNextConn(current)
		if next == nil || next == current {
			break
		}
		current = next
	}

	return nil
}

func getNextConn(conn net.Conn) net.Conn {
	connValue := reflect.ValueOf(conn)
	if connValue.Kind() == reflect.Ptr {
		connElem := connValue.Elem()
		if connElem.IsValid() && connElem.Kind() == reflect.Struct {
			if extField := connElem.FieldByName("ExtendedConn"); extField.IsValid() && !extField.IsNil() {
				extConn := extField.Interface()
				if upstream, ok := extConn.(common.WithUpstream); ok {
					if next, ok := upstream.Upstream().(net.Conn); ok && next != nil {
						return next
					}
				}
				if next, ok := extConn.(net.Conn); ok {
					return next
				}
			}

			if connField := connElem.FieldByName("Conn"); connField.IsValid() {
				if connField.Kind() == reflect.Interface && !connField.IsNil() {
					if next, ok := connField.Interface().(net.Conn); ok {
						return next
					}
				}
			}
		}
	}

	if upstream, ok := conn.(common.WithUpstream); ok {
		if next, ok := upstream.Upstream().(net.Conn); ok && next != nil {
			return next
		}
	}

	return nil
}

func (c *VisionConn) SetDirectConn(conn net.Conn) {
	if conn == nil {
		return
	}
	c.writeAccess.Lock()
	defer c.writeAccess.Unlock()
	c.directConn = conn
	if c.directWrite {
		c.writer = bufio.NewVectorisedWriter(conn)
	}
}

func (c *VisionConn) SetUplinkConn(conn net.Conn) {
	c.SetDirectConn(conn)
}

type netConnProvider interface {
	NetConn() net.Conn
}

func unwrapConn(conn net.Conn) net.Conn {
	visited := make(map[net.Conn]struct{})
	for conn != nil {
		if _, ok := visited[conn]; ok {
			break
		}
		visited[conn] = struct{}{}

		// Check if this is a TLS connection by testing with TLS registry
		if isTLSConn(conn) {
			break
		}

		if isEncryptionConn(conn) {
			break
		}

		switched := false

		// Try common.WithUpstream first (most specific)
		if upstream, ok := conn.(common.WithUpstream); ok {
			if next, ok := upstream.Upstream().(net.Conn); ok && next != nil && next != conn {
				conn = next
				switched = true
				continue
			}
		}

		// Then try netConnProvider
		if provider, ok := conn.(netConnProvider); ok {
			next := provider.NetConn()
			if next != nil && next != conn {
				conn = next
				switched = true
				continue
			}
		}

		if reader, ok := conn.(N.WithUpstreamReader); ok {
			if replacer, ok := conn.(N.ReaderWithUpstream); ok && replacer.ReaderReplaceable() {
				if next, ok := reader.UpstreamReader().(net.Conn); ok && next != nil && next != conn {
					conn = next
					switched = true
					continue
				}
			}
		}
		if writer, ok := conn.(N.WithUpstreamWriter); ok {
			if replacer, ok := conn.(N.WriterWithUpstream); ok && replacer.WriterReplaceable() {
				if next, ok := writer.UpstreamWriter().(net.Conn); ok && next != nil && next != conn {
					conn = next
					switched = true
					continue
				}
			}
		}
		if !switched {
			break
		}
	}
	return conn
}

// isTLSConn checks if a connection can be recognized by the TLS registry
func isTLSConn(conn net.Conn) bool {
	for _, tlsCreator := range tlsRegistry {
		loaded, _, _, _ := tlsCreator(conn)
		if loaded {
			return true
		}
	}
	return false
}

func (c *VisionConn) Read(p []byte) (n int, err error) {
	for len(c.remainingBuffers) > 0 {
		newN, _ := c.remainingBuffers[0].Read(p[n:])
		if c.remainingBuffers[0].IsEmpty() {
			c.remainingBuffers[0].Release()
			c.remainingBuffers = c.remainingBuffers[1:]
		}
		n += newN
		if n == len(p) {
			break
		}
	}
	if n > 0 {
		return
	}
	if c.directRead {
		return c.directConn.Read(p)
	}
	var bufferBytes []byte
	var chunkBuffer *buf.Buffer
	if len(p) > xrayChunkSize {
		n, err = c.Conn.Read(p)
		if err != nil {
			return
		}
		bufferBytes = p[:n]
	} else {
		chunkBuffer, err = c.reader.ReadChunk()
		if err != nil {
			return 0, err
		}
		bufferBytes = chunkBuffer.Bytes()
	}
	if c.withinPaddingBuffers || c.numberOfPacketToFilter > 0 {
		buffers := c.unPadding(bufferBytes)
		if chunkBuffer != nil {
			chunkBuffer.Reset()
		}
		if c.remainingContent == 0 && c.remainingPadding == 0 {
			if c.currentCommand == commandPaddingEnd {
				c.withinPaddingBuffers = false
				c.remainingContent = -1
				c.remainingPadding = -1
			} else if c.currentCommand == commandPaddingDirect {
				c.withinPaddingBuffers = false
				c.directRead = true

				inputBuffer, err := io.ReadAll(c.input)
				if err != nil {
					return 0, err
				}
				buffers = append(buffers, buf.As(inputBuffer))

				rawInputBuffer, err := io.ReadAll(c.rawInput)
				if err != nil {
					return 0, err
				}

				buffers = append(buffers, buf.As(rawInputBuffer))

				c.logger.Trace("XtlsRead readV")
			} else if c.currentCommand == commandPaddingContinue {
				c.withinPaddingBuffers = true
			} else {
				return 0, E.New("unknown command ", c.currentCommand)
			}
		} else if c.remainingContent > 0 || c.remainingPadding > 0 {
			c.withinPaddingBuffers = true
		} else {
			c.withinPaddingBuffers = false
		}
		if c.numberOfPacketToFilter > 0 {
			c.filterTLS(buf.ToSliceMulti(buffers))
		}
		c.remainingBuffers = buffers
		return c.Read(p)
	} else {
		if c.numberOfPacketToFilter > 0 {
			c.filterTLS([][]byte{bufferBytes})
		}
		if chunkBuffer != nil {
			c.remainingBuffers = append(c.remainingBuffers, buf.As(chunkBuffer.Bytes()))
			chunkBuffer.Reset() // chunkBuffer should not be release and only reused after c.remainingBuffers be emptied, so must reset at here
			return c.Read(p)
		}
		return
	}
}

func (c *VisionConn) Write(p []byte) (n int, err error) {
	c.writeAccess.Lock()
	defer c.writeAccess.Unlock()
	if c.numberOfPacketToFilter > 0 {
		c.filterTLS([][]byte{p})
	}
	if c.isPadding {
		inputLen := len(p)
		isComplete := isCompleteRecord(p)
		buffers := reshapeBuffer(p)
		switchToDirectAfterWrite := false
		for i, buffer := range buffers {
			if c.isTLS && buffer.Len() > 6 && bytes.Equal(tlsApplicationDataStart, buffer.To(3)) && isComplete {
				if c.enableXTLS && c.canSplice {
					switchToDirectAfterWrite = true
				}
				c.isPadding = false
				if i == len(buffers)-1 {
					var command byte = commandPaddingEnd
					if switchToDirectAfterWrite {
						command = commandPaddingDirect
					}
					buffers[i] = c.padding(buffer, command)
				} else {
					buffers[i] = c.padding(buffer, commandPaddingContinue)
				}
				continue
			} else if !c.isTLS12orAbove && c.numberOfPacketToFilter <= 1 {
				c.isPadding = false
				buffers[i] = c.padding(buffer, commandPaddingEnd)
				break
			}
			var command byte = commandPaddingContinue
			if i == len(buffers)-1 && !c.isPadding {
				command = commandPaddingEnd
				if switchToDirectAfterWrite {
					command = commandPaddingDirect
				}
			}
			buffers[i] = c.padding(buffer, command)
		}
		err = c.writer.WriteVectorised(buffers)
		if err == nil {
			n = inputLen
			if switchToDirectAfterWrite {
				c.directWrite = true
				c.writer = bufio.NewVectorisedWriter(c.directConn)
			}
		}
		return
	}
	if c.directWrite {
		return c.directConn.Write(p)
	} else {
		return c.Conn.Write(p)
	}
}

func (c *VisionConn) filterTLS(buffers [][]byte) {
	for _, buffer := range buffers {
		c.numberOfPacketToFilter--
		if len(buffer) > 6 {
			if buffer[0] == 22 && buffer[1] == 3 && buffer[2] == 3 {
				c.isTLS = true
				if buffer[5] == 2 {
					c.isTLS12orAbove = true
					c.remainingServerHello = (int32(buffer[3])<<8 | int32(buffer[4])) + 5
					if len(buffer) >= 79 && c.remainingServerHello >= 79 {
						sessionIdLen := int32(buffer[43])
						cipherSuite := buffer[43+sessionIdLen+1 : 43+sessionIdLen+3]
						c.cipher = uint16(cipherSuite[0])<<8 | uint16(cipherSuite[1])
					} else {
						c.logger.Trace("XtlsFilterTls short server hello, tls 1.2 or older? ", len(buffer), " ", c.remainingServerHello)
					}
				}
			} else if bytes.Equal(tlsClientHandShakeStart, buffer[:2]) && buffer[5] == 1 {
				c.isTLS = true
				c.logger.Trace("XtlsFilterTls found tls client hello! ", len(buffer))
			}
		}
		if c.remainingServerHello > 0 {
			end := int(c.remainingServerHello)
			if end > len(buffer) {
				end = len(buffer)
			}
			c.remainingServerHello -= int32(end)
			if bytes.Contains(buffer[:end], tls13SupportedVersions) {
				cipher, ok := tls13CipherSuiteDic[c.cipher]
				if ok && cipher != "TLS_AES_128_CCM_8_SHA256" {
					c.enableXTLS = true
				}
				c.logger.Trace("XtlsFilterTls found tls 1.3! ", len(buffer), " ", c.cipher, " ", c.enableXTLS)
				c.numberOfPacketToFilter = 0
				return
			} else if c.remainingServerHello == 0 {
				c.logger.Trace("XtlsFilterTls found tls 1.2! ", len(buffer))
				c.numberOfPacketToFilter = 0
				return
			}
		}
		if c.numberOfPacketToFilter == 0 {
			c.logger.Trace("XtlsFilterTls stop filtering ", len(buffer))
		}
	}
}

func (c *VisionConn) padding(buffer *buf.Buffer, command byte) *buf.Buffer {
	contentLen := 0
	paddingLen := 0
	if buffer != nil {
		contentLen = buffer.Len()
	}
	if contentLen < 900 && c.isTLS {
		l, _ := rand.Int(rand.Reader, big.NewInt(500))
		paddingLen = int(l.Int64()) + 900 - contentLen
	} else {
		l, _ := rand.Int(rand.Reader, big.NewInt(256))
		paddingLen = int(l.Int64())
	}
	var bufferLen int
	if c.writeUUID {
		bufferLen += 16
	}
	bufferLen += 5
	if buffer != nil {
		bufferLen += buffer.Len()
	}
	bufferLen += paddingLen
	newBuffer := buf.NewSize(bufferLen)
	if c.writeUUID {
		common.Must1(newBuffer.Write(c.userUUID[:]))
		c.writeUUID = false
	}
	common.Must1(newBuffer.Write([]byte{command, byte(contentLen >> 8), byte(contentLen), byte(paddingLen >> 8), byte(paddingLen)}))
	if buffer != nil {
		common.Must1(newBuffer.Write(buffer.Bytes()))
		buffer.Release()
	}
	newBuffer.Extend(paddingLen)
	c.logger.Trace("XtlsPadding ", contentLen, " ", paddingLen, " ", command)
	return newBuffer
}

func (c *VisionConn) unPadding(buffer []byte) []*buf.Buffer {
	var bufferIndex int
	if c.remainingContent == -1 && c.remainingPadding == -1 {
		if len(buffer) >= 21 && bytes.Equal(c.userUUID[:], buffer[:16]) {
			bufferIndex = 16
			c.remainingContent = 0
			c.remainingPadding = 0
			c.currentCommand = 0
		}
	}
	if c.remainingContent == -1 && c.remainingPadding == -1 {
		return []*buf.Buffer{buf.As(buffer).ToOwned()}
	}
	var buffers []*buf.Buffer
	for bufferIndex < len(buffer) {
		if c.remainingContent <= 0 && c.remainingPadding <= 0 {
			if c.currentCommand == 1 {
				buffers = append(buffers, buf.As(buffer[bufferIndex:]).ToOwned())
				break
			} else {
				paddingInfo := buffer[bufferIndex : bufferIndex+5]
				c.currentCommand = paddingInfo[0]
				c.remainingContent = int(paddingInfo[1])<<8 | int(paddingInfo[2])
				c.remainingPadding = int(paddingInfo[3])<<8 | int(paddingInfo[4])
				bufferIndex += 5
				c.logger.Trace("Xtls Unpadding new block ", bufferIndex, " ", c.remainingContent, " padding ", c.remainingPadding, " ", c.currentCommand)
			}
		} else if c.remainingContent > 0 {
			end := c.remainingContent
			if end > len(buffer)-bufferIndex {
				end = len(buffer) - bufferIndex
			}
			buffers = append(buffers, buf.As(buffer[bufferIndex:bufferIndex+end]).ToOwned())
			c.remainingContent -= end
			bufferIndex += end
		} else {
			end := c.remainingPadding
			if end > len(buffer)-bufferIndex {
				end = len(buffer) - bufferIndex
			}
			c.remainingPadding -= end
			bufferIndex += end
		}
		if bufferIndex == len(buffer) {
			break
		}
	}
	return buffers
}

func (c *VisionConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *VisionConn) Upstream() any {
	return c.Conn
}

func (c *VisionConn) ReaderReplaceable() bool {
	return c.directRead
}

func (c *VisionConn) WriterReplaceable() bool {
	c.writeAccess.Lock()
	defer c.writeAccess.Unlock()
	return c.directWrite
}

func (c *VisionConn) UpstreamReader() any {
	if c.directRead {
		return c.directConn
	}
	return nil
}

func (c *VisionConn) UpstreamWriter() any {
	c.writeAccess.Lock()
	defer c.writeAccess.Unlock()
	if c.directWrite {
		return c.directConn
	}
	return nil
}
