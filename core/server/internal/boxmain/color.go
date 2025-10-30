package boxmain

import (
	"github.com/sagernet/sing-box/option"
	"time"

	"github.com/sagernet/sing-box/log"
)

func DisableColor() {
	disableColor = true
	factory, _ := log.New(log.Options{Options: option.LogOptions{Output: "stderr", DisableColor: disableColor}, BaseTime: time.Now()})
	log.SetStdLogger(factory.Logger())
}
