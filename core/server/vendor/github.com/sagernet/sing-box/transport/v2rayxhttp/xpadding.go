package xhttp

import (
	"crypto/rand"
	"math"
	"net/http"
	"net/url"
	"strings"

	"github.com/sagernet/sing-box/option"
	"golang.org/x/net/http2/hpack"
)

type PaddingMethod string

const (
	PaddingMethodRepeatX  PaddingMethod = "repeat-x"
	PaddingMethodTokenish PaddingMethod = "tokenish"
)

const charsetBase62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
const avgHuffmanBytesPerCharBase62 = 0.8
const validationTolerance = 2

type XPaddingPlacement struct {
	Placement string
	Key       string
	Header    string
	RawURL    string
}

type XPaddingConfig struct {
	Length    int
	Placement XPaddingPlacement
	Method    PaddingMethod
}

func randStringFromCharset(n int, charset string) (string, bool) {
	if n <= 0 || len(charset) == 0 {
		return "", false
	}
	m := len(charset)
	limit := byte(256 - (256 % m))
	result := make([]byte, n)
	i := 0
	buf := make([]byte, 256)
	for i < n {
		if _, err := rand.Read(buf); err != nil {
			return "", false
		}
		for _, rb := range buf {
			if rb >= limit {
				continue
			}
			result[i] = charset[int(rb)%m]
			i++
			if i == n {
				break
			}
		}
	}
	return string(result), true
}

func absInt(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

func GenerateTokenishPaddingBase62(targetHuffmanBytes int) string {
	n := int(math.Ceil(float64(targetHuffmanBytes) / avgHuffmanBytesPerCharBase62))
	if n < 1 {
		n = 1
	}
	randBase62Str, ok := randStringFromCharset(n, charsetBase62)
	if !ok {
		return ""
	}
	const maxIter = 150
	adjustChar := byte('X')
	for iter := 0; iter < maxIter; iter++ {
		currentLength := int(hpack.HuffmanEncodeLength(randBase62Str))
		diff := currentLength - targetHuffmanBytes
		if absInt(diff) <= validationTolerance {
			return randBase62Str
		}
		if diff < 0 {
			randBase62Str += string(adjustChar)
			if adjustChar == 'X' {
				adjustChar = 'Z'
			} else {
				adjustChar = 'X'
			}
		} else {
			if len(randBase62Str) <= 1 {
				return randBase62Str
			}
			randBase62Str = randBase62Str[:len(randBase62Str)-1]
		}
	}
	return randBase62Str
}

func GeneratePadding(method PaddingMethod, length int) string {
	if length <= 0 {
		return ""
	}
	switch method {
	case PaddingMethodRepeatX:
		return strings.Repeat("X", length)
	case PaddingMethodTokenish:
		paddingValue := GenerateTokenishPaddingBase62(length)
		if paddingValue == "" {
			return strings.Repeat("X", length)
		}
		return paddingValue
	default:
		return strings.Repeat("X", length)
	}
}

func ApplyPaddingToCookie(req *http.Request, name, value string) {
	if req == nil || name == "" || value == "" {
		return
	}
	req.AddCookie(&http.Cookie{Name: name, Value: value, Path: "/"})
}

func ApplyPaddingToQuery(u *url.URL, key, value string) {
	if u == nil || key == "" || value == "" {
		return
	}
	q := u.Query()
	q.Set(key, value)
	u.RawQuery = q.Encode()
}

func ApplyXPaddingToHeader(h http.Header, config XPaddingConfig) {
	if h == nil {
		return
	}
	paddingValue := GeneratePadding(config.Method, config.Length)
	switch p := config.Placement; p.Placement {
	case option.PlacementHeader:
		h.Set(p.Header, paddingValue)
	case option.PlacementQueryInHeader:
		u, err := url.Parse(p.RawURL)
		if err != nil || u == nil {
			return
		}
		u.RawQuery = p.Key + "=" + paddingValue
		h.Set(p.Header, u.String())
	}
}

func ApplyXPaddingToRequest(req *http.Request, config XPaddingConfig) {
	if req == nil {
		return
	}
	if req.Header == nil {
		req.Header = make(http.Header)
	}
	placement := config.Placement.Placement
	if placement == option.PlacementHeader || placement == option.PlacementQueryInHeader {
		ApplyXPaddingToHeader(req.Header, config)
		return
	}
	paddingValue := GeneratePadding(config.Method, config.Length)
	switch placement {
	case option.PlacementCookie:
		ApplyPaddingToCookie(req, config.Placement.Key, paddingValue)
	case option.PlacementQuery:
		ApplyPaddingToQuery(req.URL, config.Placement.Key, paddingValue)
	}
}

func ExtractXPaddingFromRequest(options *option.V2RayXHTTPBaseOptions, req *http.Request, obfsMode bool) (string, string) {
	if req == nil {
		return "", ""
	}
	if !obfsMode {
		referrer := req.Header.Get("Referer")
		if referrer != "" {
			if referrerURL, err := url.Parse(referrer); err == nil {
				paddingValue := referrerURL.Query().Get("x_padding")
				paddingPlacement := option.PlacementQueryInHeader + "=Referer, key=x_padding"
				return paddingValue, paddingPlacement
			}
		} else {
			paddingValue := req.URL.Query().Get("x_padding")
			return paddingValue, option.PlacementQuery + ", key=x_padding"
		}
	}
	key := options.XPaddingKey
	header := options.XPaddingHeader
	if cookie, err := req.Cookie(key); err == nil {
		if cookie != nil && cookie.Value != "" {
			paddingValue := cookie.Value
			paddingPlacement := option.PlacementCookie + ", key=" + key
			return paddingValue, paddingPlacement
		}
	}
	headerValue := req.Header.Get(header)
	if headerValue != "" {
		if options.XPaddingPlacement == option.PlacementHeader {
			paddingPlacement := option.PlacementHeader + "=" + header
			return headerValue, paddingPlacement
		}
		if parsedURL, err := url.Parse(headerValue); err == nil {
			paddingPlacement := option.PlacementQueryInHeader + "=" + header + ", key=" + key
			return parsedURL.Query().Get(key), paddingPlacement
		}
	}
	queryValue := req.URL.Query().Get(key)
	if queryValue != "" {
		paddingPlacement := option.PlacementQuery + ", key=" + key
		return queryValue, paddingPlacement
	}
	return "", ""
}

func IsPaddingValid(options *option.V2RayXHTTPBaseOptions, paddingValue string, from, to int32, method PaddingMethod) bool {
	if paddingValue == "" {
		return false
	}
	if to <= 0 {
		r := options.GetNormalizedXPaddingBytes()
		from, to = r.From, r.To
	}
	switch method {
	case PaddingMethodRepeatX:
		n := int32(len(paddingValue))
		return n >= from && n <= to
	case PaddingMethodTokenish:
		const tolerance = int32(validationTolerance)
		n := int32(hpack.HuffmanEncodeLength(paddingValue))
		f := from - tolerance
		t := to + tolerance
		if f < 0 {
			f = 0
		}
		return n >= f && n <= t
	default:
		n := int32(len(paddingValue))
		return n >= from && n <= to
	}
}
