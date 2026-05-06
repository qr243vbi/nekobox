package alidns

import (
	"crypto/hmac"
	"crypto/sha1"
	"encoding/base64"
	"errors"
	"fmt"
	"net/http"
	"sort"
	"strings"
	"time"
)

func defaultSchemaV2(cred *CredentialInfo, scheme string) (*aliClientSchema, error) {
	if cred == nil {
		return &aliClientSchema{}, errors.New("alidns: credentials missing")
	}
	if scheme == "" {
		scheme = "http"
	}

	return &aliClientSchema{
		APIHost: fmt.Sprintf(addressOfAPI, scheme),
		requestPairs: []keyPair{
			{Key: "AccessKeyId", Value: cred.AccessKeyID},
			{Key: "Format", Value: "JSON"},
			{Key: "SignatureMethod", Value: "HMAC-SHA1"},
			{Key: "SignatureNonce", Value: fmt.Sprintf("%d", time.Now().UnixNano())},
			{Key: "SignatureVersion", Value: "1.0"},
			{Key: "Timestamp", Value: time.Now().UTC().Format("2006-01-02T15:04:05Z")},
			{Key: "Version", Value: "2015-01-09"},
		},
		version:      2,
		signString:   "",
		signPassword: cred.AccessKeySecret,
	}, nil
}

func signStrV2(src string, secret string) string {
	secret = secret + "&"
	hm := hmac.New(sha1.New, []byte(secret))
	hm.Write([]byte(src))
	sum := hm.Sum(nil)
	return base64.StdEncoding.EncodeToString(sum)
}

func (c *aliClientSchema) reqStrToSignV2(src string, method string) string {
	if method == "" {
		method = http.MethodGet
	}
	return fmt.Sprintf("%s&%s&%s", method, "%2F", urlEncode(src))
}

func (c *aliClientSchema) signReqV2(method string) error {
	sort.Sort(keyPairs(c.requestPairs))
	str := c.reqMapToStr()
	str = c.reqStrToSignV2(str, method)
	c.signString = signStrV2(str, c.signPassword)
	return nil
}

func (c *aliClientSchema) setActionV2(action string) error {
	err := c.UpsertRequestBody("Action", action)
	if err != nil {
		return err
	}
	return nil
}

func (c *aliClientSchema) urlV2(src string) string {
	si0 := fmt.Sprintf("%s=%s", "Signature", strings.ReplaceAll(c.signString, "+", "%2B"))
	return fmt.Sprintf("%s&%s", src, si0)
}
