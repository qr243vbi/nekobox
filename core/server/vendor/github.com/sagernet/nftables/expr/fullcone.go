package expr

import (
	"github.com/mdlayher/netlink"
	"golang.org/x/sys/unix"
)

type FullCone struct{}

func (e *FullCone) marshal(fam byte) ([]byte, error) {
	return netlink.MarshalAttributes([]netlink.Attribute{
		{Type: unix.NFTA_EXPR_NAME, Data: []byte("fullcone\x00")},
		{Type: unix.NLA_F_NESTED | unix.NFTA_EXPR_DATA, Data: []byte{}},
	})
}

func (e *FullCone) unmarshal(fam byte, data []byte) error {
	return nil
}
