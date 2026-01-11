package network

type EarlyConn interface {
	NeedHandshake() bool
}
