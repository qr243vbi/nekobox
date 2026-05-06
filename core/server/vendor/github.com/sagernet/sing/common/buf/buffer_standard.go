//go:build !with_low_memory

package buf

const (
	BufferSize    = 32 * 1024
	UDPBufferSize = 16 * 1024
)
