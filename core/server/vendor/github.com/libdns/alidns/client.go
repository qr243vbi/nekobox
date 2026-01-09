package alidns

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"sync"
)

// mClient is an abstration of AliClient
type mClient struct {
	aClient *aliClient
	mutex   sync.Mutex
}

// TODO:Will complete,If we need to get Domain Info for something.
func (c *mClient) getDomainInfo(ctx context.Context, zone string) error {
	return nil
}

func (c *mClient) doAPIRequest(ctx context.Context, method string, result interface{}) error {
	req, err := c.applyReq(ctx, method, nil)
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
	strBody := string(buf)
	if err != nil {
		return err
	}

	err = json.Unmarshal([]byte(strBody), result)
	if err != nil {
		return err
	}
	if rsp.StatusCode != 200 {
		return fmt.Errorf("get error status: HTTP %d: %+v", rsp.StatusCode, result.(*aliResult).Msg)
	}
	c.aClient = nil
	return err
}
