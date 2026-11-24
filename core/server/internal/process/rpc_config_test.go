package process_test

import (
	"context"
	"testing"

	"github.com/apache/thrift/lib/go/thrift"
	"nekobox_core/gen"
)

func TestAuxiliaryCoreRPCAndLegacyCompatibility(t *testing.T) {
	ctx := context.Background()
	for _, auxiliary := range []bool{false, true} {
		request := &gen.LoadConfigReq{NeedExtraProcess: true, ExtraProcessPath: "custom-core.exe"}
		if auxiliary {
			request.ExtraProcesses = []*gen.ExtraProcessConfig{{
				Path: "xray.exe", Args_: "run -format json -c %s", Config: "synthetic-json",
				ConfigDir: "directory with spaces", NoOut: true,
			}}
		}
		wire, err := thrift.NewTSerializer().Write(ctx, request)
		if err != nil {
			t.Fatal(err)
		}
		decoded := new(gen.LoadConfigReq)
		if err := thrift.NewTDeserializer().Read(ctx, decoded, wire); err != nil {
			t.Fatal(err)
		}
		if decoded.ExtraProcessPath != request.ExtraProcessPath || !decoded.NeedExtraProcess {
			t.Fatal("legacy primary process fields changed")
		}
		if auxiliary {
			if len(decoded.ExtraProcesses) != 1 || *decoded.ExtraProcesses[0] != *request.ExtraProcesses[0] {
				t.Fatal("auxiliary process did not survive RPC round trip")
			}
		} else if decoded.IsSetExtraProcesses() {
			t.Fatal("legacy request acquired auxiliary processes")
		}
	}
}
