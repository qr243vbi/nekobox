package boxmain

import (
	"github.com/sagernet/sing-box/option"
	"os"
	"time"

	_ "github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/log"

	"github.com/spf13/cobra"
)

var (
	configPaths       []string
	configDirectories []string
	workingDir        string
	disableColor      bool
	//
	protectListenPath string
	protectFwMark     int
)

var mainCommand = &cobra.Command{
	Use:              "sing-box",
	PersistentPreRun: preRun,
}

func init() {
	mainCommand.PersistentFlags().StringArrayVarP(&configPaths, "config", "c", nil, "set configuration file path")
	mainCommand.PersistentFlags().StringArrayVarP(&configDirectories, "config-directory", "C", nil, "set configuration directory path")
	mainCommand.PersistentFlags().StringVarP(&workingDir, "directory", "D", "", "set working directory")
	mainCommand.PersistentFlags().BoolVarP(&disableColor, "disable-color", "", false, "disable color output")
	mainCommand.PersistentFlags().StringVarP(&protectListenPath, "protect-listen-path", "", "", "Linux Only")
	mainCommand.PersistentFlags().IntVarP(&protectFwMark, "protect-fwmark", "", 0, "Linux Only")
}

func Main() {
	if err := mainCommand.Execute(); err != nil {
		log.Fatal(err)
	}
}

func preRun(cmd *cobra.Command, args []string) {
	if disableColor {
		factory, _ := log.New(log.Options{Options: option.LogOptions{Output: "stderr", DisableColor: true}, BaseTime: time.Now()})
		log.SetStdLogger(factory.Logger())
	}
	if workingDir != "" {
		_, err := os.Stat(workingDir)
		if err != nil {
			os.MkdirAll(workingDir, 0o777)
		}
		if err := os.Chdir(workingDir); err != nil {
			log.Fatal(err)
		}
	}
	if len(configPaths) == 0 && len(configDirectories) == 0 {
		configPaths = append(configPaths, "config.json")
	}
}
