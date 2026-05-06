package alidns

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"
)

// aliClient is an abstration of AliClient
type aliClient struct {
	schema          *aliClientSchema
	DomainName      string
	InstanceEdition instanceEdition
	mutex           sync.Mutex
}

func (c *aliClient) IsEntprienseEdition() bool {
	return c.InstanceEdition.IsEnterpriseEdition()
}

func getClient(cred *CredentialInfo, zone ...string) (*aliClient, error) {
	result := &aliClient{}
	schema, err := getClientSchema(cred, "https")
	if err != nil {
		return result, err
	}
	result.schema = schema
	if len(zone) == 0 {
		return result, nil
	}
	tmp, _ := getClient(cred)
	info, err := tmp.queryDomainInfo(context.Background(), strings.Trim(zone[0], "."))
	if err != nil {
		return result, err
	}
	if len(info.DomainName) > 0 {
		result.DomainName = info.DomainName
	}
	if len(info.VersionCode) > 0 {
		result.InstanceEdition = info.VersionCode
	}
	return result, nil
}

func (c *aliClient) SetAction(action string) error {
	if c.schema == nil {
		return errors.New("schema was not initialed proprely")
	}
	return c.schema.SetAction(action)
}

func (c *aliClient) SetRequestBody(key string, value string) error {
	if c.schema == nil {
		return errors.New("schema was not initialed proprely")
	}
	return c.schema.UpsertRequestBody(key, value)
}

func (c *aliClient) Lock() {
	c.mutex.Lock()
}

func (c *aliClient) Unlock() {
	c.mutex.Unlock()
}

func (c *aliClient) doAPIRequest(ctx context.Context, result interface{}, methods ...string) error {
	method := http.MethodPost
	if len(methods) > 0 {
		method = methods[0]
	}
	req, err := c.schema.HttpRequest(ctx, method)
	if err != nil {
		return err
	}

	rsp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}
	defer rsp.Body.Close()

	var buf []byte
	buf, err = io.ReadAll(rsp.Body)
	if err != nil {
		return err
	}

	err = json.Unmarshal(buf, result)
	if err != nil {
		return err
	}
	if rsp.StatusCode != 200 {
		return fmt.Errorf("get error status: HTTP %d: %+v", rsp.StatusCode, result.(*aliDomainResult).Msg)
	}
	c.schema = nil
	return err
}
