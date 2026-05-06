package shadowtls

const (
	tlsRandomSize    = 32
	tlsHeaderSize    = 5
	tlsSessionIDSize = 32

	clientHello = 1
	serverHello = 2

	changeCipherSpec = 20
	alert            = 21
	handshake        = 22
	applicationData  = 23

	serverRandomIndex    = tlsHeaderSize + 1 + 3 + 2
	sessionIDLengthIndex = tlsHeaderSize + 1 + 3 + 2 + tlsRandomSize
	tlsHmacHeaderSize    = tlsHeaderSize + hmacSize
	hmacSize             = 4
)
