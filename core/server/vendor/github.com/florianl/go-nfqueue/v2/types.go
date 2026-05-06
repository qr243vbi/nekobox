package nfqueue

import (
	"errors"
	"time"
)

// Attribute contains various elements for nfqueue elements.
// As not every value is contained in every nfqueue message,
// the elements inside Attribute are pointers to these values
// or nil, if not present.
type Attribute struct {
	PacketID   *uint32
	Hook       *uint8
	Timestamp  *time.Time
	Mark       *uint32
	InDev      *uint32
	PhysInDev  *uint32
	OutDev     *uint32
	PhysOutDev *uint32
	Payload    *[]byte
	CapLen     *uint32
	UID        *uint32
	GID        *uint32
	SecCtx     *string
	L2Hdr      *[]byte
	HwAddr     *[]byte
	HwProtocol *uint16
	Ct         *[]byte
	CtInfo     *uint32
	SkbInfo    *[]byte
	Exp        *[]byte
	SkbPrio    *uint32
}

// HookFunc is a function, that receives events from a Netlinkgroup
// To stop receiving messages on this HookFunc, return something different than 0.
type HookFunc func(a Attribute) int

// ErrorFunc is a function that receives all errors that happen while reading
// from a Netlinkgroup. To stop receiving messages return something different than 0.
type ErrorFunc func(e error) int

// Config contains options for a Conn.
type Config struct {
	// Network namespace the Nfqueue needs to operate in. If set to 0 (default),
	// no network namespace will be entered.
	NetNS int

	// Queue this Nfqueue socket will be assigned to
	NfQueue uint16
	// Maximum number of packages within the Nfqueue.
	MaxQueueLen uint32

	// Only used in combination with NfQnlCopyPacket.
	MaxPacketLen uint32

	// Specifies how the kernel handles a packet in the nfqueue queue.
	Copymode uint8

	// Optional flags for this Nfqueue socket.
	Flags uint32

	// AfFamily for this Nfqueue socket.
	AfFamily uint8

	// Deprecated: Cancel the context passed to RegisterWithErrorFunc() or Register()
	// to remove the hook from the nfqueue gracefully.
	ReadTimeout time.Duration

	// Time till a write action times out - only available for Go >= 1.12
	WriteTimeout time.Duration

	// Interface to log internals.
	Logger Logger
}

// Various errors
var (
	ErrRecvMsg        = errors.New("received error message")
	ErrUnexpMsg       = errors.New("received unexpected message from kernel")
	ErrInvFlag        = errors.New("invalid Flag")
	ErrNotLinux       = errors.New("not implemented for OS other than linux")
	ErrInvalidVerdict = errors.New("invalid verdict")
)

// nfLogSubSysQueue the netlink subsystem we will query
const nfnlSubSysQueue = 0x03

const (
	nfQaUnspec = iota
	nfQaPacketHdr
	nfQaVerdictHdr        /* nfqnl_msg_verdict_hrd */
	nfQaMark              /* __u32 nfmark */
	nfQaTimestamp         /* nfqnl_msg_packet_timestamp */
	nfQaIfIndexInDev      /* __u32 ifindex */
	nfQaIfIndexOutDev     /* __u32 ifindex */
	nfQaIfIndexPhysInDev  /* __u32 ifindex */
	nfQaIfIndexPhysOutDev /* __u32 ifindex */
	nfQaHwAddr            /* nfqnl_msg_packet_hw */
	nfQaPayload           /* opaque data payload */
	nfQaCt                /* nf_conntrack_netlink.h */
	nfQaCtInfo            /* enum ip_conntrack_info */
	nfQaCapLen            /* __u32 length of captured packet */
	nfQaSkbInfo           /* __u32 skb meta information */
	nfQaExp               /* nf_conntrack_netlink.h */
	nfQaUID               /* __u32 sk uid */
	nfQaGID               /* __u32 sk gid */
	nfQaSecCtx            /* security context string */
	nfQaVLAN              /* nested attribute: packet vlan info */
	nfQaL2HDR             /* full L2 header */
	nfQaPriority          /* skb->priority */
)

const (
	_                  = iota
	nfQaCfgCmd         /* nfqnl_msg_config_cmd */
	nfQaCfgParams      /* nfqnl_msg_config_params */
	nfQaCfgQueueMaxLen /* __u32 */
	nfQaCfgMask        /* identify which flags to change */
	nfQaCfgFlags       /* value of these flags (__u32) */
)

const (
	_ = iota
	nfUlnlCfgCmdBind
	nfUlnlCfgCmdUnbind
	nfUlnlCfgCmdPfBind
	nfUlnlCfgCmdPfUnbind
)

const (
	nfQnlMsgPacket       = iota
	nfQnlMsgVerdict      /* verdict from userspace to kernel */
	nfQnlMsgConfig       /* connect to a particular queue */
	nfQnlMsgVerdictBatch /* batch from userspace to kernel */

)

// Various configuration flags
const (
	NfQaCfgFlagFailOpen  = (1 << iota)
	NfQaCfgFlagConntrack = (1 << iota)
	NfQaCfgFlagGSO       = (1 << iota)
	NfQaCfgFlagUIDGid    = (1 << iota)
	NfQaCfgFlagSecCx     = (1 << iota)
	nfQaCfgFlagMax       = (1 << iota)
)

// copy modes
const (
	NfQnlCopyNone = iota
	NfQnlCopyMeta
	NfQnlCopyPacket
)

// Verdicts
const (
	NfDrop = iota
	NfAccept
	NfStolen
	NfQeueue
	NfRepeat
)

// conntrack attributes
// include/uapi/linux/netfilter/nfnetlink_conntrack.h
const (
	ctaMark   = 8
	ctaLabels = 22 // CTA_LABELS
)
