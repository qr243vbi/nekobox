package main

import (
	"context"
	"encoding/json"
	"fmt"
	"nekobox_core/internal"
	"reflect"
	"strings"

	"github.com/invopop/jsonschema"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/service/filemanager"
)

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	filemanager.MkdirAll(ctx, "fine", 0)
	cancel()

	reflector := jsonschema.Reflector{}

	reflector.Mapper = func(t reflect.Type) *jsonschema.Schema {
		str := strings.TrimLeft(t.String(), "*")
		if strings.HasPrefix(str, "badoption.") {
			str = strings.TrimPrefix(str, "badoption.")
			if str == "httpheader" {
				return nil
			}
			if strings.HasPrefix(str, "Listable[github.com/sagernet/sing-box/option.") {
				return &jsonschema.Schema{
					Ref: "#/$defs/Listable[string]",
				}
			}
			if strings.HasPrefix(str, "Listable[") {
				return nil
			}
			return &jsonschema.Schema{
				Type:   "string",
				Format: str,
			}
		}
		return nil // Return nil to let the library handle default types
	}

	schema := reflector.Reflect(&option.JuicityOutboundOptions{})

	b, _ := json.MarshalIndent(schema, "", "  ")
	fmt.Println(string(b))

	fmt.Println(internal.SetId(1, 2))
}
