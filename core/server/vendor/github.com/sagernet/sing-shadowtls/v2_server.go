package shadowtls

import (
	"bytes"
	"context"
	"encoding/binary"
	"io"
	"net"
	"os"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/logger"
)

func copyUntilHandshakeFinishedV2(ctx context.Context, logger logger.ContextLogger, dst net.Conn, src io.Reader, hash *hashWriteConn, fallbackAfter int) (*buf.Buffer, error) {
	var tlsHdr [tlsHeaderSize]byte
	var applicationDataCount int
	for {
		_, err := io.ReadFull(src, tlsHdr[:])
		if err != nil {
			return nil, err
		}
		length := binary.BigEndian.Uint16(tlsHdr[3:])
		if tlsHdr[0] == applicationData {
			data := buf.NewSize(int(length))
			_, err = data.ReadFullFrom(src, int(length))
			if err != nil {
				data.Release()
				return nil, err
			}
			if hash.HasContent() && length >= 8 {
				checksum := hash.Sum()
				if bytes.Equal(data.To(8), checksum) {
					logger.TraceContext(ctx, "match current hashcode")
					data.Advance(8)
					return data, nil
				} else if hash.LastSum() != nil && bytes.Equal(data.To(8), hash.LastSum()) {
					logger.TraceContext(ctx, "match last hashcode")
					data.Advance(8)
					return data, nil
				} else {
					logger.TraceContext(ctx, "hashcode mismatch")
				}
			}
			_, err = io.Copy(dst, io.MultiReader(bytes.NewReader(tlsHdr[:]), data))
			data.Release()
			applicationDataCount++
		} else {
			_, err = io.Copy(dst, io.MultiReader(bytes.NewReader(tlsHdr[:]), io.LimitReader(src, int64(length))))
		}
		if err != nil {
			return nil, err
		}
		if applicationDataCount > fallbackAfter {
			return nil, os.ErrPermission
		}
	}
}
