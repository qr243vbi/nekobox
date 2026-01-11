package replay

type Filter interface {
	Check(sum []byte) bool
}
