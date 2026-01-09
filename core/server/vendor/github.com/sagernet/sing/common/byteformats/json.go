package byteformats

import (
	"strconv"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
)

const (
	Byte = 1 << (iota * 10)
	KiByte
	MiByte
	GiByte
	TiByte
	PiByte
	EiByte
)

const (
	KByte = Byte * 1000
	MByte = KByte * 1000
	GByte = MByte * 1000
	TByte = GByte * 1000
	PByte = TByte * 1000
	EByte = PByte * 1000
)

var unitValueTable = map[string]uint64{
	"b":   Byte,
	"k":   KByte,
	"kb":  KByte,
	"ki":  KiByte,
	"kib": KiByte,
	"m":   MByte,
	"mb":  MByte,
	"mi":  MiByte,
	"mib": MiByte,
	"g":   GByte,
	"gb":  GByte,
	"gi":  GiByte,
	"gib": GiByte,
	"t":   TByte,
	"tb":  TByte,
	"ti":  TiByte,
	"tib": TiByte,
	"p":   PByte,
	"pb":  PByte,
	"pi":  PiByte,
	"pib": PiByte,
	"e":   EByte,
	"eb":  EByte,
	"ei":  EiByte,
	"eib": EiByte,
}

var memoryUnitValueTable = map[string]uint64{
	"b":  Byte,
	"k":  KiByte,
	"kb": KiByte,
	"m":  MiByte,
	"mb": MiByte,
	"g":  GiByte,
	"gb": GiByte,
	"t":  TiByte,
	"tb": TiByte,
	"p":  PiByte,
	"pb": PiByte,
	"e":  EiByte,
	"eb": EiByte,
}

var networkUnitValueTable = map[string]uint64{
	"Bps":  Byte,
	"Kbps": KByte / 8,
	"KBps": KByte,
	"Mbps": MByte / 8,
	"MBps": MByte,
	"Gbps": GByte / 8,
	"GBps": GByte,
	"Tbps": TByte / 8,
	"TBps": TByte,
	"Pbps": PByte / 8,
	"PBps": PByte,
	"Ebps": EByte / 8,
	"EBps": EByte,
}

type rawBytes struct {
	value     uint64
	unit      string
	unitValue uint64
}

func (b rawBytes) MarshalJSON() ([]byte, error) {
	if b.unit == "" {
		return json.Marshal(b.value)
	}
	return json.Marshal(strconv.FormatUint(b.value/b.unitValue, 10) + b.unit)
}

func parseUnit(b *rawBytes, unitTable map[string]uint64, caseSensitive bool, bytes []byte) error {
	var intValue int64
	err := json.Unmarshal(bytes, &intValue)
	if err == nil {
		b.value = uint64(intValue)
		b.unit = ""
		b.unitValue = 1
		return nil
	}
	var stringValue string
	err = json.Unmarshal(bytes, &stringValue)
	if err != nil {
		return err
	}
	unitIndex := 0
	for i, c := range stringValue {
		if c < '0' || c > '9' {
			unitIndex = i
			break
		}
	}
	if unitIndex == 0 {
		return E.New("invalid format: ", stringValue)
	}
	value, err := strconv.ParseUint(stringValue[:unitIndex], 10, 64)
	if err != nil {
		return E.Cause(err, "parse ", stringValue[:unitIndex])
	}
	rawUnit := stringValue[unitIndex:]
	var unit string
	if caseSensitive {
		unit = strings.TrimSpace(rawUnit)
	} else {
		unit = strings.TrimSpace(strings.ToLower(rawUnit))
	}
	unitValue, loaded := unitTable[unit]
	if !loaded {
		return E.New("unsupported unit: ", rawUnit)
	}
	b.value = value * unitValue
	b.unit = rawUnit
	b.unitValue = unitValue
	return nil
}

type Bytes struct {
	rawBytes
}

func (b *Bytes) Value() uint64 {
	if b == nil {
		return 0
	}
	return b.value
}

func (b *Bytes) UnmarshalJSON(bytes []byte) error {
	return parseUnit(&b.rawBytes, unitValueTable, false, bytes)
}

type MemoryBytes struct {
	rawBytes
}

func (m *MemoryBytes) Value() uint64 {
	if m == nil {
		return 0
	}
	return m.value
}

func (m *MemoryBytes) UnmarshalJSON(bytes []byte) error {
	return parseUnit(&m.rawBytes, memoryUnitValueTable, false, bytes)
}

type NetworkBytes struct {
	rawBytes
}

func (n *NetworkBytes) Value() uint64 {
	if n == nil {
		return 0
	}
	return n.value
}

func (n *NetworkBytes) UnmarshalJSON(bytes []byte) error {
	return parseUnit(&n.rawBytes, networkUnitValueTable, true, bytes)
}

type NetworkBytesCompat struct {
	rawBytes
}

func (n *NetworkBytesCompat) Value() uint64 {
	if n == nil {
		return 0
	}
	return n.value
}

func (n *NetworkBytesCompat) UnmarshalJSON(bytes []byte) error {
	err := parseUnit(&n.rawBytes, networkUnitValueTable, true, bytes)
	if err != nil {
		newErr := parseUnit(&n.rawBytes, unitValueTable, false, bytes)
		if newErr == nil {
			return nil
		}
	}
	return err
}
