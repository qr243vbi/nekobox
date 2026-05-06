package alidns

import (
	"context"
	"errors"
	"fmt"
	"strings"
)

func (c *aliClient) queryDomainInfo(ctx context.Context, zone string) (aliDomainInfo, error) {
	if c.schema == nil {
		return aliDomainInfo{}, errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	c.SetAction("DescribeDomains")
	c.SetRequestBody("KeyWord", zone)
	c.SetRequestBody("SearchMode", "EXACT")
	rs := aliDomainResult{}
	err := c.doAPIRequest(ctx, &rs)
	if err != nil {
		return aliDomainInfo{}, err
	}
	if len(rs.Domains.Domain) == 0 {
		return aliDomainInfo{}, errors.New("cannot found specified zone:" + zone)
	}
	return rs.Domains.Domain[0], err
}

func (c *aliClient) addDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	if c.schema == nil {
		return "", errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	if rc.TTL <= 0 {
		rc.TTL = 600
	}
	c.SetAction("AddDomainRecord")
	c.SetRequestBody("DomainName", rc.DomainName)
	c.SetRequestBody("RR", rc.Rr)
	c.SetRequestBody("Type", rc.DomainType)
	c.SetRequestBody("Value", rc.DomainValue)
	c.SetRequestBody("TTL", fmt.Sprintf("%d", rc.TTL))
	rs := aliDomainResult{}
	err = c.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (c *aliClient) delDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	if c.schema == nil {
		return "", errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	c.SetAction("DeleteDomainRecord")
	c.SetRequestBody("RecordId", rc.RecordID)
	rs := aliDomainResult{}
	err = c.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (c *aliClient) setDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	if c.schema == nil {
		return "", errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	if rc.TTL <= 0 {
		rc.TTL = 600
	}
	c.SetAction("UpdateDomainRecord")
	c.SetRequestBody("RecordId", rc.RecordID)
	c.SetRequestBody("RR", rc.Rr)
	c.SetRequestBody("Type", rc.DomainType)
	c.SetRequestBody("Value", rc.DomainValue)
	c.SetRequestBody("TTL", fmt.Sprintf("%d", rc.TTL))
	rs := aliDomainResult{}
	err = c.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (c *aliClient) getDomainRecord(ctx context.Context, recID string) (aliDomainRecord, error) {
	if c.schema == nil {
		return aliDomainRecord{}, errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	c.SetAction("DescribeDomainRecordInfo")
	c.SetRequestBody("RecordId", recID)
	rs := aliDomainResult{}
	err := c.doAPIRequest(ctx, &rs)
	rec := rs.ToDomaRecord()
	if err != nil {
		return aliDomainRecord{}, err
	}
	return rec, err
}

func (c *aliClient) queryDomainRecords(ctx context.Context, name string) ([]aliDomainRecord, error) {
	if c.schema == nil {
		return nil, errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	c.SetAction("DescribeDomainRecords")
	c.SetRequestBody("DomainName", strings.Trim(name, "."))
	rs := aliDomainResult{}
	err := c.doAPIRequest(ctx, &rs)
	if err != nil {
		return []aliDomainRecord{}, err
	}
	return rs.DomainRecords.Record, err
}

func (c *aliClient) queryDomainRecord(ctx context.Context, rr, name string, recType string, recVal ...string) (aliDomainRecord, error) {
	if c.schema == nil {
		return aliDomainRecord{}, errors.New("schema was not initialed proprely")
	}
	c.Lock()
	defer c.Unlock()
	c.SetAction("DescribeDomainRecords")
	c.SetRequestBody("DomainName", strings.Trim(name, "."))
	c.SetRequestBody("RRKeyWord", rr)
	if recType != "" {
		c.SetRequestBody("TypeKeyWord", recType)
	}
	if len(recVal) > 0 && recVal[0] != "" {
		c.SetRequestBody("ValueKeyWord", recVal[0])
	}
	c.SetRequestBody("SearchMode", "COMBINATION")
	rs := aliDomainResult{}
	err := c.doAPIRequest(ctx, &rs)
	if err != nil {
		return aliDomainRecord{}, err
	}
	if len(rs.DomainRecords.Record) == 0 {
		return aliDomainRecord{}, errors.New("the Record Name of the domain not found")
	}
	return rs.DomainRecords.Record[0], err
}