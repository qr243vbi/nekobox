package boxmain

import (
	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"
	"nekobox_core/internal/boxbox"

	"github.com/spf13/cobra"
)

var commandToolsFlagOutbound string

var commandTools = &cobra.Command{
	Use:   "tools",
	Short: "Experimental tools",
}

func init() {
	commandTools.PersistentFlags().StringVarP(&commandToolsFlagOutbound, "outbound", "o", "", "Use specified tag instead of default outbound")
	mainCommand.AddCommand(commandTools)
}

func createPreStartedClient() (*boxbox.Box, error) {
	options, err := readConfigAndMerge()
	if err != nil {
		return nil, err
	}
	instance, err := boxbox.New(boxbox.Options{Options: options})
	if err != nil {
		return nil, E.Cause(err, "create service")
	}
	err = instance.PreStart()
	if err != nil {
		return nil, E.Cause(err, "start service")
	}
	return instance, nil
}

func createDialer(instance *boxbox.Box, network string, outboundTag string) (N.Dialer, error) {
	if outboundTag == "" {
		outbound, _ := instance.Router().DefaultOutbound(N.NetworkName(network))
		if outbound == nil {
			return nil, E.New("missing default outbound")
		}
		return outbound, nil
	} else {
		outbound, loaded := instance.Router().Outbound(outboundTag)
		if !loaded {
			return nil, E.New("outbound not found: ", outboundTag)
		}
		return outbound, nil
	}
}
