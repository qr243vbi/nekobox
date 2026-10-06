package boxmain

import (
	"context"
	"github.com/sagernet/sing-box/include"

	"nekobox_core/internal/boxbox"
)

var ruleset_cachedir string

func Check(content []byte) error {
	ctx := include.Context(context.Background())
	options, err := parseConfig(ctx, content)
	if err != nil {
		return err
	}
	ctx, cancel := context.WithCancel(ctx)
	instance, err := boxbox.New(boxbox.Options{
		Context: ctx,
		Options: *options,
	})
	if err == nil {
		instance.Close()
	}
	cancel()
	return err
}
