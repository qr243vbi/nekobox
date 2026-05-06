package alidns

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"math/rand"
	"net/http"
	"net/url"
	"sort"
	"strings"
	"time"
)

func defaultSchemaV3(cred *CredentialInfo, scheme string) (*aliClientSchema, error) {
	if cred == nil {
		return &aliClientSchema{}, errors.New("alidns: credentials missing")
	}
	if scheme == "" {
		scheme = "http"
	}
	result := &aliClientSchema{
		APIHost: fmt.Sprintf(addressOfAPI, scheme),
		headerPairs: []keyPair{
			{Key: "x-acs-version", Value: "2015-01-09"},
			{Key: "x-acs-date", Value: time.Now().UTC().Format(time.RFC3339)},
			{Key: "x-acs-signature-nonce",
				Value: fmt.Sprintf("%d", time.Now().UnixNano()+rand.Int63())},
		},
		requestPairs: []keyPair{},
		version:      3,
		signString:   "ACS3-HMAC-SHA256 Credential=" + cred.AccessKeyID,
		signPassword: cred.AccessKeySecret,
	}
	if len(cred.SecurityToken) > 0 {
		_ = result.UpsertHeader("x-acs-security-token", cred.SecurityToken)
	}
	return result, nil
}

func hmacStringV3(src string, secret string) string {
	hm := hmac.New(sha256.New, []byte(secret))
	hm.Write([]byte(src))
	sum := hm.Sum(nil)
	return hex.EncodeToString(sum)
}

func hashString(src string) string {
	hash := sha256.New()
	hash.Write([]byte(src))
	return hex.EncodeToString(hash.Sum(nil))
}

func (c *aliClientSchema) setActionV3(action string) error {
	err := c.UpsertHeader("x-acs-action", action)
	if err != nil {
		return err
	}
	return nil
}

func (c *aliClientSchema) requestToSignV3(method string) string {
	mUrl, _ := url.Parse(c.APIHost)
	_ = c.UpsertHeader("host", mUrl.Host)
	hashedRequestBody := hashString("")
	if method == http.MethodPost {
		hashedRequestBody = hashString(c.requestPairs.UrlEncodedString())
		_ = c.UpsertHeader("content-type", "application/x-www-form-urlencoded")
	}
	_ = c.UpsertHeader("x-acs-content-sha256", hashedRequestBody)
	var headersToSign keyPairs
	for _, el := range c.headerPairs {
		needToSign := strings.HasPrefix(el.Key, "x-acs-") ||
			el.Key == "content-type" ||
			el.Key == "host"
		if !needToSign {
			continue
		}
		headersToSign, _ = headersToSign.Upsert(el.Key, el.Value)
	}
	headerKeysToSign := ""
	sort.Sort(headersToSign)
	for _, k := range headersToSign.Keys() {
		headerKeysToSign += k + ";"
	}
	headerKeysToSign = strings.TrimSuffix(headerKeysToSign, ";")
	headersStringToSign := headersToSign.SplitToString(":", "\n")

	sort.Sort(c.requestPairs)
	c.signString += ",SignedHeaders=" + headerKeysToSign
	result := strings.ToUpper(method) + "\n" +
		mUrl.Path + "\n"
	if method == http.MethodGet {
		result += c.requestPairs.PercentCodeString()
	}
	result += "\n"
	result += headersStringToSign + "\n\n" +
		headerKeysToSign + "\n" +
		hashedRequestBody
	return result
}

func (c *aliClientSchema) signReqV3(method string) error {
	requestString := c.requestToSignV3(method)
	stringToSign := "ACS3-HMAC-SHA256" + "\n" + hashString(requestString)
	c.signString += ",Signature=" + strings.ToLower(hmacStringV3(stringToSign, c.signPassword))
	_ = c.UpsertHeader("Authorization", c.signString)
	return nil
}
