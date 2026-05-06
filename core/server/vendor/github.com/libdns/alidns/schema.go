package alidns

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"math"
	"net/http"
	"net/url"
	"runtime"
	"strconv"
	"strings"
	"sync"
)

const defaultRegionID string = "cn-hangzhou"
const addressOfAPI string = "%s://alidns.aliyuncs.com/"

// CredentialInfo implements param of the crediential
type CredentialInfo struct {
	// The API Key ID Required by Aliyun's for accessing the Aliyun's API
	AccessKeyID string `json:"access_key_id"`
	// The API Key Secret Required by Aliyun's for accessing the Aliyun's API
	AccessKeySecret string `json:"access_key_secret"`
	// Optional for identifing the region of the Aliyun's Service,The default is zh-hangzhou
	RegionID string `json:"region_id,omitempty"`
	// The Security Token Required If you enabled the Aliyun's STS(SecurityToken) for accessing the Aliyun's API
	SecurityToken string `json:"security_token,omitempty"`
}

// aliClientSchema abstructs the alidns.Client
type aliClientSchema struct {
	mutex        sync.Mutex
	APIHost      string
	headerPairs  keyPairs
	requestPairs keyPairs
	signString   string
	signPassword string
	version      int
}

// keyPair implments of K-V struct
type keyPair struct {
	Key   string
	Value string
}

func getClientSchema(cred *CredentialInfo, scheme string) (*aliClientSchema, error) {
	if cred.AccessKeyID == "" || cred.AccessKeySecret == "" {
		return nil, errors.New("empty AccessKeyID or AccessKeySecret")
	}
	if len(cred.RegionID) == 0 {
		cred.RegionID = defaultRegionID
	}
	return defaultSchemaV3(cred, scheme)
}

func (c *aliClientSchema) signReq(method string) error {
	if c.signPassword == "" || len(c.requestPairs) == 0 {
		return errors.New("alidns: AccessKeySecret or Request(includes AccessKeyId) is Misssing")
	}
	switch c.version {
	case 2:
		return c.signReqV2(method)
	default:
		return c.signReqV3(method)
	}
}

func (c *aliClientSchema) UpsertHeader(key string, value string) error {
	c.mutex.Lock()
	var err error
	c.headerPairs, err = c.headerPairs.Upsert(key, value)
	if err != nil {
		c.mutex.Unlock()
		return err
	}
	c.mutex.Unlock()
	return nil
}

func (c *aliClientSchema) UpsertRequestBody(key string, value string) error {
	c.mutex.Lock()
	var err error
	c.requestPairs, err = c.requestPairs.Upsert(key, value)
	if err != nil {
		c.mutex.Unlock()
		return err
	}
	c.mutex.Unlock()
	return nil
}

func (c *aliClientSchema) reqMapToStr() string {
	c.mutex.Lock()
	result := c.requestPairs.UrlEncodedString()
	c.mutex.Unlock()
	return result
}

func (c *aliClientSchema) SetAction(action string) error {
	if len(action) == 0 {
		return errors.New("empty action to set")
	}
	switch c.version {
	case 2:
		return c.setActionV2(action)
	default:
		return c.setActionV3(action)
	}
}

// HttpRequest generates http.Request from schema
func (c *aliClientSchema) HttpRequest(cxt context.Context, method string) (*http.Request, error) {
	if method == "" {
		method = http.MethodGet
	}
	c.signReq(method)
	requestUrl := c.APIHost
	if method == http.MethodGet {
		requestUrl = fmt.Sprintf("%s?%s", requestUrl, c.reqMapToStr())
	}
	if c.version == 2 {
		requestUrl = c.urlV2(requestUrl)
	}
	var bodyReader io.Reader
	if method == http.MethodPost && c.version > 2 {
		bodyReader = bytes.NewReader([]byte(c.reqMapToStr()))
	}
	req, err := http.NewRequestWithContext(cxt, method, requestUrl, bodyReader)
	if err != nil {
		return &http.Request{}, err
	}
	req.Header.Set("Accept", "application/json")
	if len(c.headerPairs) == 0 {
		return req, nil
	}
	for _, v := range c.headerPairs {
		req.Header.Set(v.Key, v.Value)
	}
	return req, nil
}

func goVer() float64 {
	versionString := runtime.Version()
	versionString, _ = strings.CutPrefix(versionString, "go")
	verStrings := strings.Split(versionString, ".")
	var result float64
	for i, v := range verStrings {
		tmp, _ := strconv.ParseFloat(v, 32)
		result = tmp * (1 / math.Pow10(i))
	}
	return result
}

func percentCode(src string) string {
	result := strings.Replace(src, "+", "%20", -1)
	result = strings.Replace(result, "*", "%2A", -1)
	result = strings.Replace(result, "%7E", "~", -1)
	return result
}

func urlEncode(src string) string {
	str0 := url.QueryEscape(src)
	str0 = percentCode(str0)
	if goVer() > 1.20 {
		str0 = strings.Replace(str0, "%26", "&", -1)
	}

	return str0
}

type keyPairs []keyPair

func (p keyPairs) Upsert(key, value string) (keyPairs, error) {
	if key == "" || value == "" {
		return p, errors.New("key or value is Empty")
	}
	srcEl := keyPair{Key: key, Value: value}
	for i, el := range p {
		if srcEl.Key == el.Key {
			p[i] = srcEl
			return p, nil
		}
	}
	p = append(p, srcEl)
	return p, nil
}

func (p keyPairs) SplitToString(pair, pairs string) string {
	result := ""
	if len(pair) == 0 {
		pair = ":"
	}
	if len(pairs) == 0 {
		pairs = ","
	}
	for _, el := range p {
		result += el.Key +
			pair +
			el.Value +
			pairs
	}
	return strings.TrimSuffix(result, pairs)
}

func (p keyPairs) PercentCodeString() string {
	if len(p) == 0 {
		return ""
	}
	var tmp keyPairs
	for _, v := range p {
		tmp, _ = tmp.Upsert(urlEncode(v.Key), urlEncode(v.Value))
	}
	return tmp.SplitToString("=", "&")
}

func (p keyPairs) UrlEncodedString() string {
	if len(p) == 0 {
		return ""
	}
	tmp := url.Values{}
	for _, el := range p {
		tmp.Add(el.Key, el.Value)
	}
	return tmp.Encode()
}

func (p keyPairs) Keys() []string {
	result := make([]string, p.Len())
	for index, el := range p {
		result[index] = el.Key
	}
	return result
}

func (p keyPairs) Len() int {
	return len(p)
}

func (p keyPairs) Swap(i, j int) {
	p[i], p[j] = p[j], p[i]
}

func (p keyPairs) Less(i, j int) bool {
	return p[i].Key < p[j].Key
}
