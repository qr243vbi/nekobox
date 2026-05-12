package main_sing

import (
        "github.com/sagernet/sing-box/log"
)

func MainFunc() {
        if err := mainCommand.Execute(); err != nil {
                log.Fatal(err)
        }
}
