// modify from https://github.com/XTLS/REALITY/tree/e26ae2305463dd69cccc8a79a3576d7b68c4f3a4

package tls

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/ed25519"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/sha512"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"math/big"
	"net"
	"runtime"
	"sync"
	"time"

	"github.com/metacubex/utls/internal/mlkem"
	"github.com/metacubex/utls/internal/ratelimit"

	"golang.org/x/crypto/curve25519"
	"golang.org/x/crypto/hkdf"
)

type realityMirrorConn struct {
	*sync.Mutex
	net.Conn
	Target net.Conn
}

func (c *realityMirrorConn) Read(b []byte) (int, error) {
	c.Unlock()
	runtime.Gosched()
	n, err := c.Conn.Read(b)
	c.Lock() // calling c.Lock() before c.Target.Write(), to make sure that this goroutine has the priority to make the next move
	if n != 0 {
		c.Target.Write(b[:n])
	}
	if err != nil {
		c.Target.Close()
	}
	return n, err
}

func (c *realityMirrorConn) Write(b []byte) (int, error) {
	return 0, fmt.Errorf("Write(%v)", len(b))
}

func (c *realityMirrorConn) Close() error {
	return fmt.Errorf("Close()")
}

func (c *realityMirrorConn) SetDeadline(t time.Time) error {
	return nil
}

func (c *realityMirrorConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (c *realityMirrorConn) SetWriteDeadline(t time.Time) error {
	return nil
}

const realitySize uint16 = 8192

var realityTypes = [7]string{
	"Server Hello",
	"Change Cipher Spec",
	"Encrypted Extensions",
	"Certificate",
	"Certificate Verify",
	"Finished",
	"New Session Ticket",
}

func realityValue(vals ...byte) (value int) {
	for i, val := range vals {
		value |= int(val) << ((len(vals) - i - 1) * 8)
	}
	return
}

type RealityLimitFallback struct {
	AfterBytes       uint64
	BytesPerSec      uint64
	BurstBytesPerSec uint64
}

type RealityConfig struct {
	DialContext func(ctx context.Context, network, address string) (net.Conn, error)

	Log  func(format string, v ...any)
	Type string
	Dest string
	Xver byte

	ServerNames  map[string]bool
	PrivateKey   []byte
	MinClientVer []byte
	MaxClientVer []byte
	MaxTimeDiff  time.Duration
	ShortIds     map[[8]byte]bool

	LimitFallbackUpload   RealityLimitFallback
	LimitFallbackDownload RealityLimitFallback

	Config
}

func (a *RealityConfig) Clone() *RealityConfig {
	return &RealityConfig{
		DialContext:           a.DialContext,
		Log:                   a.Log,
		Type:                  a.Type,
		Dest:                  a.Dest,
		Xver:                  a.Xver,
		ServerNames:           a.ServerNames,
		PrivateKey:            a.PrivateKey,
		MinClientVer:          a.MinClientVer,
		MaxClientVer:          a.MaxClientVer,
		MaxTimeDiff:           a.MaxTimeDiff,
		ShortIds:              a.ShortIds,
		LimitFallbackUpload:   a.LimitFallbackUpload,
		LimitFallbackDownload: a.LimitFallbackDownload,
		Config:                *a.Config.Clone(),
	}
}

type rateLimitedConn struct {
	net.Conn
	After  int64
	Bucket *ratelimit.Bucket
}

func (c *rateLimitedConn) Read(b []byte) (int, error) {
	n, err := c.Conn.Read(b)
	if n != 0 {
		if c.After > 0 {
			c.After -= int64(n)
		} else {
			c.Bucket.Wait(int64(n))
		}
	}
	return n, err
}

func newRateLimitedConn(conn net.Conn, limit *RealityLimitFallback) net.Conn {
	if limit.BytesPerSec == 0 {
		return conn
	}

	burstBytesPerSec := limit.BurstBytesPerSec
	if burstBytesPerSec < limit.BytesPerSec {
		burstBytesPerSec = limit.BytesPerSec
	}

	return &rateLimitedConn{
		Conn:   conn,
		After:  int64(limit.AfterBytes),
		Bucket: ratelimit.NewBucketWithRate(float64(limit.BytesPerSec), int64(burstBytesPerSec)),
	}
}

var (
	ed25519Priv ed25519.PrivateKey
	signedCert  []byte
)

func init() {
	certificate := x509.Certificate{SerialNumber: &big.Int{}}
	_, ed25519Priv, _ = ed25519.GenerateKey(rand.Reader)
	signedCert, _ = x509.CreateCertificate(rand.Reader, &certificate, &certificate, ed25519.PublicKey(ed25519Priv[32:]), ed25519Priv)
}

type realityServerHandshakeStateTLS13 struct {
	serverHandshakeStateTLS13

	AuthKey       []byte
	ClientVer     [3]byte
	ClientTime    time.Time
	ClientShortId [8]byte
	Config        *RealityConfig
}

func (hs *realityServerHandshakeStateTLS13) handshake() error {
	c := hs.c
	config := hs.Config

	// For an overview of the TLS 1.3 handshake, see RFC 8446, Section 2.
	/*
		if err := hs.processClientHello(); err != nil {
			return err
		}
	*/
	{
		if config.Log != nil {
			config.Log("REALITY remoteAddr: %v using X25519MLKEM768: %v", c.RemoteAddr().String(), hs.hello.serverShare.group == X25519MLKEM768)
		}

		hs.suite = cipherSuiteTLS13ByID(hs.hello.cipherSuite)
		c.cipherSuite = hs.suite.id
		hs.transcript = hs.suite.hash.New()

		var peerData []byte
		for _, keyShare := range hs.clientHello.keyShares {
			if keyShare.group == hs.hello.serverShare.group {
				peerData = keyShare.data
				break
			}
		}

		var peerPub = peerData
		if hs.hello.serverShare.group == X25519MLKEM768 {
			peerPub = peerData[mlkem.EncapsulationKeySize768:]
		}

		key, _ := generateECDHEKey(c.config.rand(), X25519)
		copy(hs.hello.serverShare.data, key.PublicKey().Bytes())
		peerKey, _ := key.Curve().NewPublicKey(peerPub)
		hs.sharedKey, _ = key.ECDH(peerKey)

		if hs.hello.serverShare.group == X25519MLKEM768 {
			k, _ := mlkem.NewEncapsulationKey768(peerData[:mlkem.EncapsulationKeySize768])
			mlkemSharedSecret, ciphertext := k.Encapsulate()
			hs.sharedKey = append(mlkemSharedSecret, hs.sharedKey...)
			copy(hs.hello.serverShare.data, append(ciphertext, hs.hello.serverShare.data[:32]...))
		}

		c.serverName = hs.clientHello.serverName
	}
	/*
		if err := hs.checkForResumption(); err != nil {
			return err
		}
		if err := hs.pickCertificate(); err != nil {
			return err
		}
	*/
	{
		signedCert := append([]byte{}, signedCert...)

		h := hmac.New(sha512.New, hs.AuthKey)
		h.Write(ed25519Priv[32:])
		h.Sum(signedCert[:len(signedCert)-64])

		hs.cert = &Certificate{
			Certificate: [][]byte{signedCert},
			PrivateKey:  ed25519Priv,
		}
		hs.sigAlg = Ed25519
	}
	c.buffering = true
	if err := hs.sendServerParameters(); err != nil {
		return err
	}
	if err := hs.sendServerCertificate(); err != nil {
		return err
	}
	if err := hs.sendServerFinished(); err != nil {
		return err
	}
	if hs.c.out.handshakeLen[6] != 0 {
		if _, err := c.realityWriteRecord(recordTypeHandshake, []byte{typeNewSessionTicket}); err != nil {
			return err
		}
	}
	// Note that at this point we could start sending application data without
	// waiting for the client's second flight, but the application might not
	// expect the lack of replay protection of the ClientHello parameters.
	if _, err := c.flush(); err != nil {
		return err
	}

	/*
		if err := hs.readClientCertificate(); err != nil {
			return err
		}
		if err := hs.readClientFinished(); err != nil {
			return err
		}

		c.isHandshakeComplete.Store(true)
	*/

	return nil
}

// writeRecord writes a TLS record with the given type and payload to the
// connection and updates the record layer state.
// ONLY used by REALITY
func (c *Conn) realityWriteRecord(typ recordType, data []byte) (int, error) {
	c.out.Lock()
	defer c.out.Unlock()

	if typ == recordTypeHandshake && c.out.handshakeBuf != nil &&
		len(data) > 0 && data[0] != typeServerHello {
		c.out.handshakeBuf = append(c.out.handshakeBuf, data...)
		if data[0] != typeFinished {
			return len(data), nil
		}
		data = c.out.handshakeBuf
		c.out.handshakeBuf = nil
	}

	return c.writeRecordLocked(typ, data)
}

// RealityServer returns a new TLS server side connection
// using conn as the underlying transport.
// The configuration config must be non-nil and must include
// at least one certificate or else set GetCertificate.
func RealityServer(ctx context.Context, conn net.Conn, config *RealityConfig) (*Conn, error) {
	remoteAddr := conn.RemoteAddr().String()
	if config.Log != nil {
		config.Log("REALITY remoteAddr: %v", remoteAddr)
	}

	target, err := config.DialContext(ctx, config.Type, config.Dest)
	if err != nil {
		conn.Close()
		return nil, errors.New("REALITY: failed to dial dest: " + err.Error())
	}

	underlying := conn

	mutex := new(sync.Mutex)

	hs := realityServerHandshakeStateTLS13{
		serverHandshakeStateTLS13: serverHandshakeStateTLS13{
			c: &Conn{
				conn: &realityMirrorConn{
					Mutex:  mutex,
					Conn:   conn,
					Target: target,
				},
				config: &config.Config,
			},
			ctx: context.Background(),
		},
		Config: config,
	}

	copying := false

	waitGroup := new(sync.WaitGroup)
	waitGroup.Add(2)

	go func() {
		for {
			mutex.Lock()
			hs.clientHello, _, err = hs.c.readClientHello(context.Background()) // TODO: Change some rules in this function.
			if copying || err != nil || hs.c.vers != VersionTLS13 || !config.ServerNames[hs.clientHello.serverName] {
				break
			}
			var peerPub []byte
			for _, keyShare := range hs.clientHello.keyShares {
				if keyShare.group == X25519 && len(keyShare.data) == 32 {
					peerPub = keyShare.data
					break
				}
			}
			if peerPub == nil {
				for _, keyShare := range hs.clientHello.keyShares {
					if keyShare.group == X25519MLKEM768 && len(keyShare.data) == mlkem.EncapsulationKeySize768+32 {
						peerPub = keyShare.data[mlkem.EncapsulationKeySize768:]
						break
					}
				}
			}
			for peerPub != nil {
				if hs.AuthKey, err = curve25519.X25519(config.PrivateKey, peerPub); err != nil {
					break
				}
				if _, err = hkdf.New(sha256.New, hs.AuthKey, hs.clientHello.random[:20], []byte("REALITY")).Read(hs.AuthKey); err != nil {
					break
				}
				block, _ := aes.NewCipher(hs.AuthKey)
				aead, _ := cipher.NewGCM(block)
				if config.Log != nil {
					config.Log("REALITY remoteAddr: %v hs.c.AuthKey[:16]: %v AEAD: %T", remoteAddr, hs.AuthKey[:16], aead)
				}
				ciphertext := make([]byte, 32)
				plainText := make([]byte, 32)
				copy(ciphertext, hs.clientHello.sessionId)
				copy(hs.clientHello.sessionId, plainText) // hs.clientHello.sessionId points to hs.clientHello.raw[39:]
				if _, err = aead.Open(plainText[:0], hs.clientHello.random[20:], ciphertext, hs.clientHello.original); err != nil {
					break
				}
				copy(hs.clientHello.sessionId, ciphertext)
				copy(hs.ClientVer[:], plainText)
				hs.ClientTime = time.Unix(int64(binary.BigEndian.Uint32(plainText[4:])), 0)
				copy(hs.ClientShortId[:], plainText[8:])
				if config.Log != nil {
					config.Log("REALITY remoteAddr: %v hs.c.ClientVer: %v", remoteAddr, hs.ClientVer)
					config.Log("REALITY remoteAddr: %v hs.c.ClientTime: %v", remoteAddr, hs.ClientTime)
					config.Log("REALITY remoteAddr: %v hs.c.ClientShortId: %v", remoteAddr, hs.ClientShortId)
				}
				if (config.MinClientVer == nil || realityValue(hs.ClientVer[:]...) >= realityValue(config.MinClientVer...)) &&
					(config.MaxClientVer == nil || realityValue(hs.ClientVer[:]...) <= realityValue(config.MaxClientVer...)) &&
					(config.MaxTimeDiff == 0 || config.time().Sub(hs.ClientTime).Abs() <= config.MaxTimeDiff) &&
					(config.ShortIds[hs.ClientShortId]) {
					hs.c.conn = conn
				}
				break
			}
			if config.Log != nil {
				config.Log("REALITY remoteAddr: %v hs.c.conn == conn: %v", remoteAddr, hs.c.conn == conn)
			}
			break
		}
		mutex.Unlock()
		if hs.c.conn != conn {
			if config.Log != nil && hs.clientHello != nil {
				config.Log("REALITY remoteAddr: %v forwarded SNI: %v", remoteAddr, hs.clientHello.serverName)
			}
			io.Copy(target, newRateLimitedConn(underlying, &config.LimitFallbackUpload))
		}
		waitGroup.Done()
	}()

	go func() {
		s2cSaved := make([]byte, 0, realitySize)
		buf := make([]byte, realitySize)
		handshakeLen := 0
	f:
		for {
			runtime.Gosched()
			n, err := target.Read(buf)
			if n == 0 {
				if err != nil {
					conn.Close()
					waitGroup.Done()
					return
				}
				continue
			}
			mutex.Lock()
			s2cSaved = append(s2cSaved, buf[:n]...)
			if hs.c.conn != conn {
				copying = true // if the target already sent some data, just start bidirectional direct forwarding
				break
			}
			if len(s2cSaved) > int(realitySize) {
				break
			}
			for i, t := range realityTypes {
				if hs.c.out.handshakeLen[i] != 0 {
					continue
				}
				if i == 6 && len(s2cSaved) == 0 {
					break
				}
				if handshakeLen == 0 && len(s2cSaved) > recordHeaderLen {
					if realityValue(s2cSaved[1:3]...) != VersionTLS12 ||
						(i == 0 && (recordType(s2cSaved[0]) != recordTypeHandshake || s2cSaved[5] != typeServerHello)) ||
						(i == 1 && (recordType(s2cSaved[0]) != recordTypeChangeCipherSpec || s2cSaved[5] != 1)) ||
						(i > 1 && recordType(s2cSaved[0]) != recordTypeApplicationData) {
						break f
					}
					handshakeLen = recordHeaderLen + realityValue(s2cSaved[3:5]...)
				}
				if config.Log != nil {
					config.Log("REALITY remoteAddr: %v len(s2cSaved): %v %v: %v", remoteAddr, len(s2cSaved), t, handshakeLen)
				}
				if handshakeLen > int(realitySize) { // too long
					break f
				}
				if i == 1 && handshakeLen > 0 && handshakeLen != 6 {
					break f
				}
				if i == 2 && handshakeLen > 512 {
					hs.c.out.handshakeLen[i] = uint16(handshakeLen)
					hs.c.out.handshakeBuf = buf[:0]
					break
				}
				if i == 6 && handshakeLen > 0 {
					hs.c.out.handshakeLen[i] = uint16(handshakeLen)
					break
				}
				if handshakeLen == 0 || len(s2cSaved) < handshakeLen {
					mutex.Unlock()
					continue f
				}
				if i == 0 {
					hs.hello = new(serverHelloMsg)
					if !hs.hello.unmarshal(s2cSaved[recordHeaderLen:handshakeLen]) ||
						hs.hello.vers != VersionTLS12 || hs.hello.supportedVersion != VersionTLS13 ||
						cipherSuiteTLS13ByID(hs.hello.cipherSuite) == nil ||
						(!(hs.hello.serverShare.group == X25519 && len(hs.hello.serverShare.data) == 32) &&
							!(hs.hello.serverShare.group == X25519MLKEM768 && len(hs.hello.serverShare.data) == mlkem.CiphertextSize768+32)) {
						break f
					}
				}
				hs.c.out.handshakeLen[i] = uint16(handshakeLen)
				s2cSaved = s2cSaved[handshakeLen:]
				handshakeLen = 0
			}
			start := time.Now()
			err = hs.handshake()
			if config.Log != nil {
				config.Log("REALITY remoteAddr: %v hs.handshake() err: %v", remoteAddr, err)
			}
			if err != nil {
				break
			}
			go func() { // TODO: Probe target's maxUselessRecords and some time-outs in advance.
				if handshakeLen-len(s2cSaved) > 0 {
					io.ReadFull(target, buf[:handshakeLen-len(s2cSaved)])
				}
				if n, err := target.Read(buf); !hs.c.isHandshakeComplete.Load() {
					if err != nil {
						conn.Close()
					}
					if config.Log != nil {
						config.Log("REALITY remoteAddr: %v time.Since(start): %v n: %v err: %v", remoteAddr, time.Since(start), n, err)
					}
				}
			}()
			err = hs.readClientFinished()
			if config.Log != nil {
				config.Log("REALITY remoteAddr: %v hs.readClientFinished() err: %v", remoteAddr, err)
			}
			if err != nil {
				break
			}
			hs.c.isHandshakeComplete.Store(true)
			break
		}
		mutex.Unlock()
		if hs.c.out.handshakeLen[0] == 0 { // if the target sent an incorrect Server Hello, or before that
			if hs.c.conn == conn { // if we processed the Client Hello successfully but the target did not
				waitGroup.Add(1)
				go func() {
					io.Copy(target, newRateLimitedConn(underlying, &config.LimitFallbackUpload))
					waitGroup.Done()
				}()
			}
			conn.Write(s2cSaved)
			io.Copy(underlying, newRateLimitedConn(target, &config.LimitFallbackDownload))
		}
		waitGroup.Done()
	}()

	waitGroup.Wait()
	target.Close()
	if config.Log != nil {
		config.Log("REALITY remoteAddr: %v hs.c.isHandshakeComplete.Load(): %v", remoteAddr, hs.c.isHandshakeComplete.Load())
	}
	if hs.c.isHandshakeComplete.Load() {
		return hs.c, nil
	}
	conn.Close()
	return nil, errors.New("REALITY: processed invalid connection") // TODO: Add details.

	/*
		c := &Conn{
			conn:   conn,
			config: config,
		}
		c.handshakeFn = c.serverHandshake
		return c
	*/
}

// A listener implements a network listener (net.Listener) for TLS connections.
type realityListener struct {
	net.Listener
	config *RealityConfig
	conns  chan net.Conn
	err    error
}

// Accept waits for and returns the next incoming TLS connection.
// The returned connection is of type *Conn.
func (l *realityListener) Accept() (net.Conn, error) {
	/*
		c, err := l.Listener.Accept()
		if err != nil {
			return nil, err
		}
		return Server(c, l.config), nil
	*/
	if c, ok := <-l.conns; ok {
		return c, nil
	}
	return nil, l.err
}

// NewRealityListener creates a Listener which accepts connections from an inner
// Listener and wraps each connection with [Server].
// The configuration config must be non-nil and must include
// at least one certificate or else set GetCertificate.
func NewRealityListener(inner net.Listener, config *RealityConfig) net.Listener {
	l := new(realityListener)
	l.Listener = inner
	l.config = config
	{
		l.conns = make(chan net.Conn)
		go func() {
			for {
				c, err := l.Listener.Accept()
				if err != nil {
					l.err = err
					close(l.conns)
					return
				}
				go func() {
					defer func() {
						if r := recover(); r != nil {

						}
					}()
					c, err = RealityServer(context.Background(), c, l.config)
					if err == nil {
						l.conns <- c
					}
				}()
			}
		}()
	}
	return l
}
