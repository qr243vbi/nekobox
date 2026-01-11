package buf

func Get(size int) []byte {
	if size == 0 {
		return nil
	}
	return DefaultAllocator.Get(size)
}

func Put(buf []byte) error {
	return DefaultAllocator.Put(buf)
}

// Deprecated: use array instead.
func Make(size int) []byte {
	return make([]byte, size)
}
