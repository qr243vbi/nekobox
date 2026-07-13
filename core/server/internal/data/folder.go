package data

import (
	"errors"
	"nekobox_core/gen"

	"google.golang.org/protobuf/proto"
)

const AnyKind = 0
const SubscriptionKind = 1
const ProxyKind = 2
const BeanKind = 3
const GroupKind = 4
const RouteKind = 5

func checkIsRaw(kind uint64) bool {
	return kind > RouteKind
}

func getName(msg proto.Message) string {
	return string(msg.ProtoReflect().Descriptor().Name())
}

func getKind(msg proto.Message) (uint64, uint64) {
	switch m := msg.(type) {
	case *gen.Proxy:
		return ProxyKind, (uint64(m.GetId()))
	case *gen.Bean:
		return BeanKind, (uint64(m.GetId()))
	case *gen.Group:
		return GroupKind, (uint64(m.GetId()))
	case *gen.RoutingChain:
		return RouteKind, (uint64(m.GetId()))
	case *gen.Subscription:
		return SubscriptionKind, (uint64(m.GetId()))
	default:
		return AnyKind, 0
	}
}

func getKindErr(msg proto.Message) (uint64, uint64, error) {
	kind, id := getKind(msg)
	if kind == 0 {
		return kind, id, errors.New("Kind is 0")
	}
	if checkIsRaw(kind) {
		id = 0
	} else {
		if id == 0 {
			return kind, id, errors.New("Id is 0")
		}
	}
	return kind, id, nil
}
