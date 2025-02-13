package boxmain

import (
	"context"

	"github.com/sagernet/sing-box/log"
	"nekobox_core/internal/boxbox"

	"github.com/spf13/cobra"
)

var commandCheck = &cobra.Command{
	Use:   "check",
	Short: "Check configuration",
	Run: func(cmd *cobra.Command, args []string) {
		err := check()
		if err != nil {
			log.Fatal(err)
		}
	},
	Args: cobra.NoArgs,
}

func init() {
	mainCommand.AddCommand(commandCheck)
}

func check() error {
	options, err := parseConfig(nil)
	if err != nil {
		return err
	}
	ctx, cancel := context.WithCancel(context.Background())
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
