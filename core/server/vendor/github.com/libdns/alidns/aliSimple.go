package alidns

import (
	"context"
	"crypto/hmac"
	"crypto/sha1"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"math"
	"net/http"
	"net/url"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"
)

const defRegID string = "cn-hangzhou"
const addrOfAPI string = "%s://alidns.aliyuncs.com/"

// CredInfo implements param of the crediential
type CredInfo struct {
	AccKeyID     string `json:"access_key_id"`
	AccKeySecret string `json:"access_key_secret"`
	RegionID     string `json:"region_id,omitempty"`
}

// AliClient abstructs the alidns.Client
type aliClient struct {
	mutex   sync.Mutex
	APIHost string
	reqMap  []vKey
	sigStr  string
	sigPwd  string
}

// VKey implments of K-V struct
type vKey struct {
	key string
	val string
}

func newCredInfo(pAccKeyID, pAccKeySecret, pRegionID string) *CredInfo {
	if pAccKeyID == "" || pAccKeySecret == "" {
		return nil
	}
	if len(pRegionID) == 0 {
		pRegionID = defRegID
	}
	return &CredInfo{
		AccKeyID:     pAccKeyID,
		AccKeySecret: pAccKeySecret,
		RegionID:     pRegionID,
	}
}

func (c *mClient) getAliClient(cred *CredInfo, zone string) error {
	cl0, err := c.aClient.getAliClientSche(cred, "https")
	if err != nil {
		return err
	}
	c.aClient = cl0
	if zone != "" {
		c.getDomainInfo(context.Background(), strings.Trim(zone, "."))
	}
	return nil
}

func (c *mClient) applyReq(cxt context.Context, method string, body io.Reader) (*http.Request, error) {
	if method == "" {
		method = "GET"
	}
	c0 := c.aClient
	c0.signReq(method)
	si0 := fmt.Sprintf("%s=%s", "Signature", strings.ReplaceAll(c0.sigStr, "+", "%2B"))
	mURL := fmt.Sprintf("%s?%s&%s", c0.APIHost, c0.reqMapToStr(), si0)
	req, err := http.NewRequestWithContext(cxt, method, mURL, body)
	req.Header.Set("Accept", "application/json")
	if err != nil {
		return &http.Request{}, err
	}
	return req, nil
}

func (c *aliClient) getAliClientSche(cred *CredInfo, scheme string) (*aliClient, error) {
	if cred == nil {
		return &aliClient{}, errors.New("alidns: credentials missing")
	}
	if scheme == "" {
		scheme = "http"
	}

	cl0 := &aliClient{
		APIHost: fmt.Sprintf(addrOfAPI, scheme),
		reqMap: []vKey{
			{key: "AccessKeyId", val: cred.AccKeyID},
			{key: "Format", val: "JSON"},
			{key: "SignatureMethod", val: "HMAC-SHA1"},
			{key: "SignatureNonce", val: fmt.Sprintf("%d", time.Now().UnixNano())},
			{key: "SignatureVersion", val: "1.0"},
			{key: "Timestamp", val: time.Now().UTC().Format("2006-01-02T15:04:05Z")},
			{key: "Version", val: "2015-01-09"},
		},
		sigStr: "",
		sigPwd: cred.AccKeySecret,
	}

	return cl0, nil
}

func (c *aliClient) signReq(method string) error {
	if c.sigPwd == "" || len(c.reqMap) == 0 {
		return errors.New("alidns: AccessKeySecret or Request(includes AccessKeyId) is Misssing")
	}
	sort.Sort(byKey(c.reqMap))
	str := c.reqMapToStr()
	str = c.reqStrToSign(str, method)
	c.sigStr = signStr(str, c.sigPwd)
	return nil
}

func (c *aliClient) addReqBody(key string, value string) error {
	if key == "" && value == "" {
		return errors.New("key or value is Empty")
	}
	el := vKey{key: key, val: value}
	c.mutex.Lock()
	for _, el0 := range c.reqMap {
		if el.key == el0.key {
			c.mutex.Unlock()
			return errors.New("duplicate keys")
		}
	}
	c.reqMap = append(c.reqMap, el)
	c.mutex.Unlock()
	return nil
}

func (c *aliClient) setReqBody(key string, value string) error {
	if key == "" && value == "" {
		return errors.New("key or value is Empty")
	}
	el := vKey{key: key, val: value}
	c.mutex.Lock()
	for in, el0 := range c.reqMap {
		if el.key == el0.key {
			(c.reqMap)[in] = el
			c.mutex.Unlock()
			return nil
		}
	}
	c.mutex.Unlock()
	return fmt.Errorf("entry of %s not found", key)
}

func (c *aliClient) reqStrToSign(ins string, method string) string {
	if method == "" {
		method = "GET"
	}
	ecReq := urlEncode(ins)
	return fmt.Sprintf("%s&%s&%s", method, "%2F", ecReq)
}

func (c *aliClient) reqMapToStr() string {
	m0 := c.reqMap
	urlEn := url.Values{}
	c.mutex.Lock()
	for _, o := range m0 {
		urlEn.Add(o.key, o.val)
	}
	c.mutex.Unlock()
	return urlEn.Encode()
}

func signStr(ins string, sec string) string {
	sec = sec + "&"
	hm := hmac.New(sha1.New, []byte(sec))
	hm.Write([]byte(ins))
	sum := hm.Sum(nil)
	return base64.StdEncoding.EncodeToString(sum)
}

func goVer() float64 {
	verStr := runtime.Version()
	verStr, _ = strings.CutPrefix(verStr, "go")
	verStrs := strings.Split(verStr, ".")
	var result float64
	for i, v := range verStrs {
		tmp, _ := strconv.ParseFloat(v, 32)
		result = tmp * (1 / math.Pow10(i))
	}
	return result
}

func urlEncode(ins string) string {
	str0 := ins
	str0 = strings.Replace(str0, "+", "%20", -1)
	str0 = strings.Replace(str0, "*", "%2A", -1)
	str0 = strings.Replace(str0, "%7E", "~", -1)

	str0 = url.QueryEscape(str0)
	if goVer() > 1.20 {
		str0 = strings.Replace(str0, "%26", "&", -1)
	}

	return str0
}

type byKey []vKey

func (v byKey) Len() int {
	return len(v)
}

func (v byKey) Swap(i, j int) {
	v[i], v[j] = v[j], v[i]
}

func (v byKey) Less(i, j int) bool {
	return v[i].key < v[j].key
}
