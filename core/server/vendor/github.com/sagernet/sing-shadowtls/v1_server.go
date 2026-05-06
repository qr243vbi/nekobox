package shadowtls

import (
	"bytes"
	"encoding/binary"
	"io"

	E "github.com/sagernet/sing/common/exceptions"
)

func copyUntilHandshakeFinished(dst io.Writer, src io.Reader) error {
	var hasSeenChangeCipherSpec bool
	var tlsHdr [tlsHeaderSize]byte
	for {
		_, err := io.ReadFull(src, tlsHdr[:])
		if err != nil {
			return err
		}
		length := binary.BigEndian.Uint16(tlsHdr[3:])
		_, err = io.Copy(dst, io.MultiReader(bytes.NewReader(tlsHdr[:]), io.LimitReader(src, int64(length))))
		if err != nil {
			return err
		}
		if tlsHdr[0] != handshake {
			if tlsHdr[0] != changeCipherSpec {
				return E.New("unexpected tls frame type: ", tlsHdr[0])
			}
			if !hasSeenChangeCipherSpec {
				hasSeenChangeCipherSpec = true
				continue
			}
		}
		if hasSeenChangeCipherSpec {
			return nil
		}
	}
}
