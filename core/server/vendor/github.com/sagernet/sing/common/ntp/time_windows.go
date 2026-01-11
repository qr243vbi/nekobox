package ntp

import (
	"time"

	"golang.org/x/sys/windows"
)

func SetSystemTime(nowTime time.Time) error {
	nowTime = nowTime.UTC()
	var systemTime windows.Systemtime
	systemTime.Year = uint16(nowTime.Year())
	systemTime.Month = uint16(nowTime.Month())
	systemTime.Day = uint16(nowTime.Day())
	systemTime.Hour = uint16(nowTime.Hour())
	systemTime.Minute = uint16(nowTime.Minute())
	systemTime.Second = uint16(nowTime.Second())
	systemTime.Milliseconds = uint16(nowTime.UnixMilli() - nowTime.Unix()*1000)
	return setSystemTime(&systemTime)
}
