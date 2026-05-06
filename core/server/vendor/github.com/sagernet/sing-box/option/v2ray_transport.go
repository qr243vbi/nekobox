package option

import (
	"net/http"
	"strings"

	Xbadoption "github.com/sagernet/sing-box/common/xray/json/badoption"
	"github.com/sagernet/sing-box/common/xray/utils"
	C "github.com/sagernet/sing-box/constant"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/json/badjson"
	"github.com/sagernet/sing/common/json/badoption"
)

func NormalizeXHTTPMode(mode string) (string, error) {
	mode = strings.TrimSpace(mode)
	if mode == "" {
		return "auto", nil
	}
	switch mode {
	case "auto", "packet-up", "stream-up", "stream-one":
		return mode, nil
	default:
		return "", E.New("unsupported mode: ", mode)
	}
}

type _V2RayTransportOptions struct {
	Type               string                  `json:"type"`
	HTTPOptions        V2RayHTTPOptions        `json:"-"`
	WebsocketOptions   V2RayWebsocketOptions   `json:"-"`
	QUICOptions        V2RayQUICOptions        `json:"-"`
	GRPCOptions        V2RayGRPCOptions        `json:"-"`
	HTTPUpgradeOptions V2RayHTTPUpgradeOptions `json:"-"`
	XHTTPOptions       V2RayXHTTPOptions       `json:"-"`
	KCPOptions         V2RayKCPOptions         `json:"-"`
}

type V2RayTransportOptions _V2RayTransportOptions

func (o V2RayTransportOptions) MarshalJSON() ([]byte, error) {
	var v any
	switch o.Type {
	case C.V2RayTransportTypeHTTP:
		v = o.HTTPOptions
	case C.V2RayTransportTypeWebsocket:
		v = o.WebsocketOptions
	case C.V2RayTransportTypeQUIC:
		v = o.QUICOptions
	case C.V2RayTransportTypeGRPC:
		v = o.GRPCOptions
	case C.V2RayTransportTypeHTTPUpgrade:
		v = o.HTTPUpgradeOptions
	case C.V2RayTransportTypeXHTTP:
		v = o.XHTTPOptions
	case "":
		return nil, E.New("missing transport type")
	default:
		return nil, E.New("unknown transport type: " + o.Type)
	}
	return badjson.MarshallObjects((_V2RayTransportOptions)(o), v)
}

func (o *V2RayTransportOptions) UnmarshalJSON(bytes []byte) error {
	err := json.Unmarshal(bytes, (*_V2RayTransportOptions)(o))
	if err != nil {
		return err
	}
	var v any
	switch o.Type {
	case C.V2RayTransportTypeHTTP:
		v = &o.HTTPOptions
	case C.V2RayTransportTypeWebsocket:
		v = &o.WebsocketOptions
	case C.V2RayTransportTypeQUIC:
		v = &o.QUICOptions
	case C.V2RayTransportTypeGRPC:
		v = &o.GRPCOptions
	case C.V2RayTransportTypeHTTPUpgrade:
		v = &o.HTTPUpgradeOptions
	case C.V2RayTransportTypeXHTTP:
		v = &o.XHTTPOptions
	default:
		return E.New("unknown transport type: " + o.Type)
	}
	err = badjson.UnmarshallExcluded(bytes, (*_V2RayTransportOptions)(o), v)
	if err != nil {
		return err
	}
	return nil
}

type V2RayHTTPOptions struct {
	Host        badoption.Listable[string] `json:"host,omitempty"`
	Path        string                     `json:"path,omitempty"`
	Method      string                     `json:"method,omitempty"`
	Headers     badoption.HTTPHeader       `json:"headers,omitempty"`
	IdleTimeout badoption.Duration         `json:"idle_timeout,omitempty"`
	PingTimeout badoption.Duration         `json:"ping_timeout,omitempty"`
}

type V2RayWebsocketOptions struct {
	Path                string               `json:"path,omitempty"`
	Headers             badoption.HTTPHeader `json:"headers,omitempty"`
	MaxEarlyData        uint32               `json:"max_early_data,omitempty"`
	EarlyDataHeaderName string               `json:"early_data_header_name,omitempty"`
}

type V2RayQUICOptions struct{}

type V2RayGRPCOptions struct {
	ServiceName         string             `json:"service_name,omitempty"`
	IdleTimeout         badoption.Duration `json:"idle_timeout,omitempty"`
	PingTimeout         badoption.Duration `json:"ping_timeout,omitempty"`
	PermitWithoutStream bool               `json:"permit_without_stream,omitempty"`
	ForceLite           bool               `json:"-"` // for test
}

type V2RayHTTPUpgradeOptions struct {
	Host    string               `json:"host,omitempty"`
	Path    string               `json:"path,omitempty"`
	Headers badoption.HTTPHeader `json:"headers,omitempty"`
}

type V2RayXHTTPBaseOptions struct {
	Mode                 string                 `json:"mode"`
	Host                 string                 `json:"host,omitempty"`
	Path                 string                 `json:"path,omitempty"`
	Headers              map[string]string      `json:"headers,omitempty"`
	DomainStrategy       DomainStrategy         `json:"domain_strategy,omitempty"`
	XPaddingBytes        Xbadoption.Range       `json:"x_padding_bytes"`
	NoGRPCHeader         bool                   `json:"no_grpc_header,omitempty"`
	NoSSEHeader          bool                   `json:"no_sse_header,omitempty"`
	ScMaxEachPostBytes   Xbadoption.Range       `json:"sc_max_each_post_bytes"`
	ScMinPostsIntervalMs Xbadoption.Range       `json:"sc_min_posts_interval_ms"`
	ScMaxBufferedPosts   int64                  `json:"sc_max_buffered_posts,omitempty"`
	ScStreamUpServerSecs Xbadoption.Range       `json:"sc_stream_up_server_secs"`
	Xmux                 *V2RayXHTTPXmuxOptions `json:"xmux"`
	XPaddingObfsMode     bool                   `json:"x_padding_obfs_mode,omitempty"`
	XPaddingKey          string                 `json:"x_padding_key,omitempty"`
	XPaddingHeader       string                 `json:"x_padding_header,omitempty"`
	XPaddingPlacement    string                 `json:"x_padding_placement,omitempty"`
	XPaddingMethod       string                 `json:"x_padding_method,omitempty"`
	UplinkHTTPMethod     string                 `json:"uplink_http_method,omitempty"`
	SessionPlacement     string                 `json:"session_placement,omitempty"`
	SessionKey           string                 `json:"session_key,omitempty"`
	SeqPlacement         string                 `json:"seq_placement,omitempty"`
	SeqKey               string                 `json:"seq_key,omitempty"`
	UplinkDataPlacement  string                 `json:"uplink_data_placement,omitempty"`
	UplinkDataKey        string                 `json:"uplink_data_key,omitempty"`
	UplinkChunkSize      uint32                 `json:"uplink_chunk_size,omitempty"`
}

type _V2RayXHTTPOptions struct {
	Mode string `json:"mode"`
	V2RayXHTTPBaseOptions
	Download *V2RayXHTTPDownloadOptions `json:"download"`
}

type V2RayXHTTPOptions _V2RayXHTTPOptions

type V2RayXHTTPDownloadOptions struct {
	V2RayXHTTPBaseOptions
	ServerOptions
	OutboundTLSOptionsContainer
	Detour string `json:"detour,omitempty"`
}

const (
	PlacementQueryInHeader = "queryInHeader"
	PlacementCookie        = "cookie"
	PlacementHeader        = "header"
	PlacementQuery         = "query"
	PlacementPath          = "path"
	PlacementBody          = "body"
)

func (c V2RayXHTTPOptions) MarshalJSON() ([]byte, error) {
	return json.Marshal((*_V2RayXHTTPOptions)(&c))
}

func (c *V2RayXHTTPOptions) UnmarshalJSON(bytes []byte) error {
	err := json.Unmarshal(bytes, (*_V2RayXHTTPOptions)(c))
	if err != nil {
		return err
	}
	switch c.Mode {
	case "":
		c.Mode = "auto"
	case "auto", "packet-up", "stream-up", "stream-one":
	default:
		return E.New("unsupported mode: " + c.Mode)
	}
	err = checkV2RayXHTTPBaseOptions(c.Mode, &c.V2RayXHTTPBaseOptions)
	if err != nil {
		return err
	}
	if c.Download != nil {
		err = checkV2RayXHTTPBaseOptions(c.Mode, &c.Download.V2RayXHTTPBaseOptions)
		if err != nil {
			return err
		}
	}
	return nil
}

func checkV2RayXHTTPBaseOptions(mode string, options *V2RayXHTTPBaseOptions) error {
	for k := range options.Headers {
		if strings.ToLower(k) == "host" {
			return E.New(`"headers" can't contain "host"`)
		}
	}
	if options.XPaddingBytes.To == 0 {
		options.XPaddingBytes = options.GetNormalizedXPaddingBytes()
	}
	if options.XPaddingBytes.From <= 0 || options.XPaddingBytes.To <= 0 {
		return E.New("x_padding_bytes cannot be disabled")
	}
	if options.XPaddingKey == "" {
		options.XPaddingKey = "x_padding"
	}
	if options.XPaddingHeader == "" {
		options.XPaddingHeader = "X-Padding"
	}
	switch options.XPaddingPlacement {
	case "":
		options.XPaddingPlacement = "queryInHeader"
	case "cookie", "header", "query", "queryInHeader":
	default:
		return E.New("unsupported padding placement: " + options.XPaddingPlacement)
	}
	switch options.XPaddingMethod {
	case "":
		options.XPaddingMethod = "repeat-x"
	case "repeat-x", "tokenish":
	default:
		return E.New("unsupported padding method: " + options.XPaddingMethod)
	}
	switch options.UplinkDataPlacement {
	case "":
		options.UplinkDataPlacement = "body"
	case "body":
	case "cookie", "header":
		if mode != "packet-up" {
			return E.New("uplink_data_placement can be " + options.UplinkDataPlacement + " only in packet-up mode")
		}
	default:
		return E.New("unsupported uplink data placement: " + options.UplinkDataPlacement)
	}
	if options.UplinkHTTPMethod == "" {
		options.UplinkHTTPMethod = "POST"
	}
	options.UplinkHTTPMethod = strings.ToUpper(options.UplinkHTTPMethod)
	if options.UplinkHTTPMethod == "GET" && mode != "packet-up" {
		return E.New("uplink_http_method can be GET only in packet-up mode")
	}
	switch options.SessionPlacement {
	case "":
		options.SessionPlacement = "path"
	case "path", "cookie", "header", "query":
	default:
		return E.New("unsupported session placement: " + options.SessionPlacement)
	}
	switch options.SeqPlacement {
	case "":
		options.SeqPlacement = "path"
	case "path":
	case "cookie", "header", "query":
		if options.SessionPlacement == "path" {
			return E.New("seq_placement must be path when session_placement is path")
		}
	default:
		return E.New("unsupported seq placement: " + options.SeqPlacement)
	}
	if options.SessionPlacement != "path" && options.SessionKey == "" {
		switch options.SessionPlacement {
		case "cookie", "query":
			options.SessionKey = "x_session"
		case "header":
			options.SessionKey = "X-Session"
		}
	}
	if options.SeqPlacement != "path" && options.SeqKey == "" {
		switch options.SeqPlacement {
		case "cookie", "query":
			options.SeqKey = "x_seq"
		case "header":
			options.SeqKey = "X-Seq"
		}
	}
	if options.UplinkDataPlacement != "body" && options.UplinkDataKey == "" {
		switch options.UplinkDataPlacement {
		case "cookie":
			options.UplinkDataKey = "x_data"
		case "header":
			options.UplinkDataKey = "X-Data"
		}
	}
	if options.UplinkChunkSize == 0 {
		switch options.UplinkDataPlacement {
		case "cookie":
			options.UplinkChunkSize = 3 * 1024
		case "header":
			options.UplinkChunkSize = 4 * 1024
		}
	} else if options.UplinkChunkSize < 64 {
		options.UplinkChunkSize = 64
	}
	if options.Xmux != nil && options.Xmux.MaxConnections.To > 0 && options.Xmux.MaxConcurrency.To > 0 {
		return E.New("max_connections cannot be specified together with max_concurrency")
	}
	return nil
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedPath() string {
	pathAndQuery := strings.SplitN(c.Path, "?", 2)
	path := pathAndQuery[0]
	if path == "" || path[0] != '/' {
		path = "/" + path
	}
	if path[len(path)-1] != '/' {
		path = path + "/"
	}
	return path
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedQuery() string {
	pathAndQuery := strings.SplitN(c.Path, "?", 2)
	query := ""
	if len(pathAndQuery) > 1 {
		query = pathAndQuery[1]
	}
	return query
}

func (c *V2RayXHTTPBaseOptions) GetRequestHeader() http.Header {
	header := http.Header{}
	for k, v := range c.Headers {
		header.Add(k, v)
	}
	if header.Get("User-Agent") == "" {
		header.Set("User-Agent", utils.ChromeUA)
	}
	return header
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedXPaddingBytes() Xbadoption.Range {
	if c.XPaddingBytes.To == 0 {
		return Xbadoption.Range{
			From: 100,
			To:   1000,
		}
	}
	return c.XPaddingBytes
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedUplinkHTTPMethod() string {
	if c.UplinkHTTPMethod == "" {
		return "POST"
	}
	return c.UplinkHTTPMethod
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedScMaxEachPostBytes() Xbadoption.Range {
	if c.ScMaxEachPostBytes.To == 0 {
		return Xbadoption.Range{
			From: 1000000,
			To:   1000000,
		}
	}
	return c.ScMaxEachPostBytes
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedScMinPostsIntervalMs() Xbadoption.Range {
	if c.ScMinPostsIntervalMs.To == 0 {
		return Xbadoption.Range{
			From: 30,
			To:   30,
		}
	}
	return c.ScMinPostsIntervalMs
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedScMaxBufferedPosts() int {
	if c.ScMaxBufferedPosts == 0 {
		return 30
	}

	return int(c.ScMaxBufferedPosts)
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedScStreamUpServerSecs() Xbadoption.Range {
	if c.ScStreamUpServerSecs.To == 0 {
		return Xbadoption.Range{
			From: 20,
			To:   80,
		}
	}
	return c.ScStreamUpServerSecs
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedSessionPlacement() string {
	if c.SessionPlacement == "" {
		return PlacementPath
	}
	return c.SessionPlacement
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedSeqPlacement() string {
	if c.SeqPlacement == "" {
		return PlacementPath
	}
	return c.SeqPlacement
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedUplinkDataPlacement() string {
	if c.UplinkDataPlacement == "" {
		return PlacementBody
	}
	return c.UplinkDataPlacement
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedSessionKey() string {
	if c.SessionKey != "" {
		return c.SessionKey
	}
	switch c.GetNormalizedSessionPlacement() {
	case PlacementHeader:
		return "X-Session"
	case PlacementCookie, PlacementQuery:
		return "x_session"
	default:
		return ""
	}
}

func (c *V2RayXHTTPBaseOptions) GetNormalizedSeqKey() string {
	if c.SeqKey != "" {
		return c.SeqKey
	}
	switch c.GetNormalizedSeqPlacement() {
	case PlacementHeader:
		return "X-Seq"
	case PlacementCookie, PlacementQuery:
		return "x_seq"
	default:
		return ""
	}
}

type V2RayXHTTPXmuxOptions struct {
	MaxConcurrency   Xbadoption.Range `json:"max_concurrency"`
	MaxConnections   Xbadoption.Range `json:"max_connections"`
	CMaxReuseTimes   Xbadoption.Range `json:"c_max_reuse_times"`
	HMaxRequestTimes Xbadoption.Range `json:"h_max_request_times"`
	HMaxReusableSecs Xbadoption.Range `json:"h_max_reusable_secs"`
	HKeepAlivePeriod int64            `json:"h_keep_alive_period"`
}

func (m V2RayXHTTPXmuxOptions) isZero() bool {
	return m == (V2RayXHTTPXmuxOptions{})
}

func (m *V2RayXHTTPXmuxOptions) Validate() error {
	if m.MaxConnections.To > 0 && m.MaxConcurrency.To > 0 {
		return E.New("maxConnections cannot be specified together with maxConcurrency")
	}
	return nil
}

func (m *V2RayXHTTPXmuxOptions) GetNormalizedMaxConcurrency() Xbadoption.Range {
	if m.isZero() {
		return Xbadoption.Range{From: 1, To: 1}
	}
	return m.MaxConcurrency
}

func (m *V2RayXHTTPXmuxOptions) GetNormalizedMaxConnections() Xbadoption.Range {
	return m.MaxConnections
}

func (m *V2RayXHTTPXmuxOptions) GetNormalizedCMaxReuseTimes() Xbadoption.Range {
	return m.CMaxReuseTimes
}

func (m *V2RayXHTTPXmuxOptions) GetNormalizedHMaxRequestTimes() Xbadoption.Range {
	if m.isZero() && m.HMaxRequestTimes.From == 0 && m.HMaxRequestTimes.To == 0 {
		return Xbadoption.Range{From: 600, To: 900}
	}
	return m.HMaxRequestTimes
}

func (m *V2RayXHTTPXmuxOptions) GetNormalizedHMaxReusableSecs() Xbadoption.Range {
	if m.isZero() && m.HMaxReusableSecs.From == 0 && m.HMaxReusableSecs.To == 0 {
		return Xbadoption.Range{From: 1800, To: 3000}
	}
	return m.HMaxReusableSecs
}

type V2RayKCPOptions struct {
	MTU              uint32 `json:"mtu,omitempty"`
	TTI              uint32 `json:"tti,omitempty"`
	UplinkCapacity   uint32 `json:"uplink_capacity,omitempty"`
	DownlinkCapacity uint32 `json:"downlink_capacity,omitempty"`
	Congestion       bool   `json:"congestion,omitempty"`
	ReadBufferSize   uint32 `json:"read_buffer_size,omitempty"`
	WriteBufferSize  uint32 `json:"write_buffer_size,omitempty"`
	HeaderType       string `json:"header_type,omitempty"`
	Seed             string `json:"seed,omitempty"`
}

func (k *V2RayKCPOptions) GetMTU() uint32 {
	if k.MTU == 0 {
		return 1350
	}
	return k.MTU
}

func (k *V2RayKCPOptions) GetTTI() uint32 {
	if k.TTI == 0 {
		return 50
	}
	return k.TTI
}

func (k *V2RayKCPOptions) GetUplinkCapacity() uint32 {
	if k.UplinkCapacity == 0 {
		return 12
	}
	return k.UplinkCapacity
}

func (k *V2RayKCPOptions) GetDownlinkCapacity() uint32 {
	if k.DownlinkCapacity == 0 {
		return 100
	}
	return k.DownlinkCapacity
}

func (k *V2RayKCPOptions) GetReadBufferSize() uint32 {
	if k.ReadBufferSize == 0 {
		return 1
	}
	return k.ReadBufferSize
}

func (k *V2RayKCPOptions) GetWriteBufferSize() uint32 {
	if k.WriteBufferSize == 0 {
		return 1
	}
	return k.WriteBufferSize
}

func (k *V2RayKCPOptions) GetHeaderType() string {
	if k.HeaderType == "" {
		return "none"
	}
	return k.HeaderType
}
