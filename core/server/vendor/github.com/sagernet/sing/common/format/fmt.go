package format

import (
	"strconv"

	"github.com/sagernet/sing/common"
)

type Stringer interface {
	String() string
}

func ToString(messages ...any) string {
	var output string
	for _, rawMessage := range messages {
		if rawMessage == nil {
			output += "nil"
			continue
		}
		switch message := rawMessage.(type) {
		case string:
			output += message
		case bool:
			if message {
				output += "true"
			} else {
				output += "false"
			}
		case uint:
			output += strconv.FormatUint(uint64(message), 10)
		case uint8:
			output += strconv.FormatUint(uint64(message), 10)
		case uint16:
			output += strconv.FormatUint(uint64(message), 10)
		case uint32:
			output += strconv.FormatUint(uint64(message), 10)
		case uint64:
			output += strconv.FormatUint(message, 10)
		case int:
			output += strconv.FormatInt(int64(message), 10)
		case int8:
			output += strconv.FormatInt(int64(message), 10)
		case int16:
			output += strconv.FormatInt(int64(message), 10)
		case int32:
			output += strconv.FormatInt(int64(message), 10)
		case int64:
			output += strconv.FormatInt(message, 10)
		case uintptr:
			output += strconv.FormatUint(uint64(message), 10)
		case error:
			output += message.Error()
		case Stringer:
			output += message.String()
		default:
			panic("unknown value")
		}
	}
	return output
}

func ToString0[T any](message T) string {
	return ToString(message)
}

func MapToString[T any](arr []T) []string {
	return common.Map(arr, ToString0[T])
}

func Seconds(seconds float64) string {
	seconds100 := int(seconds * 100)
	return ToString(seconds100/100, ".", seconds100%100, seconds100%10)
}
