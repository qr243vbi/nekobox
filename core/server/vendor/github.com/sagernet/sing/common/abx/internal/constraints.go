package internal

var ProtocolMagicVersion0 = []byte{0x41, 0x42, 0x58, 0x00}

const (
	StartDocument         = 0
	EndDocument           = 1
	StartTag              = 2
	EndTag                = 3
	TEXT                  = 4
	CDSECT                = 5
	EntityRef             = 6
	IgnorableWhitespace   = 7
	ProcessingInstruction = 8
	COMMENT               = 9
	DOCDECL               = 10
	ATTRIBUTE             = 15
	TypeNull              = 1 << 4
	TypeString            = 2 << 4
	TypeStringInterned    = 3 << 4
	TypeBytesHex          = 4 << 4
	TypeBytesBase64       = 5 << 4
	TypeInt               = 6 << 4
	TypeIntHex            = 7 << 4
	TypeLong              = 8 << 4
	TypeLongHex           = 9 << 4
	TypeFloat             = 10 << 4
	TypeDouble            = 11 << 4
	TypeBooleanTrue       = 12 << 4
	TypeBooleanFalse      = 13 << 4
	MaxUnsignedShort      = 65535
)
