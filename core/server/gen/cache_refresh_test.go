package gen

import (
	"context"
	"reflect"
	"testing"

	"github.com/apache/thrift/lib/go/thrift"
)

func refreshValue(t *testing.T, request *CacheURLRequest) (bool, bool) {
	t.Helper()
	accessor, ok := any(request).(interface {
		GetRefresh() bool
		IsSetRefresh() bool
	})
	if !ok {
		t.Fatal("generated request is missing optional refresh accessors")
	}
	return accessor.GetRefresh(), accessor.IsSetRefresh()
}

func setRefresh(t *testing.T, request *CacheURLRequest, value bool) {
	t.Helper()
	field := reflect.ValueOf(request).Elem().FieldByName("Refresh")
	if !field.IsValid() || field.Type() != reflect.TypeOf((*bool)(nil)) {
		t.Fatal("generated optional refresh must be *bool")
	}
	field.Set(reflect.ValueOf(&value))
}

func requestBytes(t *testing.T, request *CacheURLRequest) []byte {
	t.Helper()
	buffer := thrift.NewTMemoryBuffer()
	protocol := thrift.NewTBinaryProtocolConf(buffer, nil)
	if err := request.Write(context.Background(), protocol); err != nil {
		t.Fatal(err)
	}
	return append([]byte(nil), buffer.Bytes()...)
}

func readRequest(t *testing.T, data []byte) *CacheURLRequest {
	t.Helper()
	buffer := thrift.NewTMemoryBuffer()
	if _, err := buffer.Write(data); err != nil {
		t.Fatal(err)
	}
	request := NewCacheURLRequest()
	if err := request.Read(context.Background(), thrift.NewTBinaryProtocolConf(buffer, nil)); err != nil {
		t.Fatal(err)
	}
	return request
}

func TestCacheRequestRefreshDefaultsAndRoundTrip(t *testing.T) {
	for _, mode := range []string{"missing", "false", "true"} {
		t.Run(mode, func(t *testing.T) {
			request := NewCacheURLRequest()
			request.HTTPURL = "https://cache.invalid/rules.srs"
			if mode != "missing" {
				setRefresh(t, request, mode == "true")
			}
			result := readRequest(t, requestBytes(t, request))
			value, set := refreshValue(t, result)
			if value != (mode == "true") || set != (mode != "missing") {
				t.Fatalf("optional field value=%v set=%v", value, set)
			}
			if result.Clear || result.HTTPURL != request.HTTPURL {
				t.Fatal("refresh changed existing request fields")
			}
			if !request.Equals(result) {
				t.Fatal("wire roundtrip changed request equality")
			}
		})
	}
}

func TestRefreshWireFieldIsOptionalAndDoesNotSetClear(t *testing.T) {
	for _, present := range []bool{false, true} {
		t.Run(map[bool]string{false: "missing", true: "present"}[present], func(t *testing.T) {
			request := NewCacheURLRequest()
			request.HTTPURL = "https://cache.invalid/rules.srs"
			if present {
				setRefresh(t, request, true)
			}
			buffer := thrift.NewTMemoryBuffer()
			buffer.Write(requestBytes(t, request))
			protocol := thrift.NewTBinaryProtocolConf(buffer, nil)
			ctx := context.Background()
			if _, err := protocol.ReadStructBegin(ctx); err != nil {
				t.Fatal(err)
			}
			sawRefresh := false
			for {
				_, kind, id, err := protocol.ReadFieldBegin(ctx)
				if err != nil {
					t.Fatal(err)
				}
				if kind == thrift.STOP {
					break
				}
				if id == 2 {
					value, err := protocol.ReadBool(ctx)
					if err != nil || value {
						t.Fatalf("refresh wire requested destructive clear: %v %v", value, err)
					}
				} else if id == 5 {
					if kind != thrift.BOOL {
						t.Fatalf("refresh field type=%v", kind)
					}
					value, err := protocol.ReadBool(ctx)
					if err != nil || !value {
						t.Fatalf("refresh wire value=%v %v", value, err)
					}
					sawRefresh = true
				} else if err := protocol.Skip(ctx, kind); err != nil {
					t.Fatal(err)
				}
				if err := protocol.ReadFieldEnd(ctx); err != nil {
					t.Fatal(err)
				}
			}
			if err := protocol.ReadStructEnd(ctx); err != nil {
				t.Fatal(err)
			}
			if sawRefresh != present {
				t.Fatalf("optional field presence=%v want=%v", sawRefresh, present)
			}
		})
	}
}

func TestCacheRequestIgnoresWrongRefreshWireType(t *testing.T) {
	buffer := thrift.NewTMemoryBuffer()
	protocol := thrift.NewTBinaryProtocolConf(buffer, nil)
	ctx := context.Background()
	if err := protocol.WriteStructBegin(ctx, "CacheURLRequest"); err != nil {
		t.Fatal(err)
	}
	if err := protocol.WriteFieldBegin(ctx, "refresh", thrift.STRING, 5); err != nil {
		t.Fatal(err)
	}
	if err := protocol.WriteString(ctx, "true"); err != nil {
		t.Fatal(err)
	}
	if err := protocol.WriteFieldEnd(ctx); err != nil {
		t.Fatal(err)
	}
	if err := protocol.WriteFieldStop(ctx); err != nil {
		t.Fatal(err)
	}
	if err := protocol.WriteStructEnd(ctx); err != nil {
		t.Fatal(err)
	}
	value, set := refreshValue(t, readRequest(t, buffer.Bytes()))
	if value || set {
		t.Fatal("invalid refresh wire field accepted")
	}
}
