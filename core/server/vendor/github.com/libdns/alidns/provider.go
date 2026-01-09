package alidns

import (
	"context"
	"errors"
	"fmt"
	"strings"

	"github.com/libdns/libdns"
)

// Provider implements the libdns interfaces for Alicloud.
type Provider struct {
	client mClient
	// The API Key ID Required by Aliyun's for accessing the Aliyun's API
	AccKeyID string `json:"access_key_id"`
	// The API Key Secret Required by Aliyun's for accessing the Aliyun's API
	AccKeySecret string `json:"access_key_secret"`
	// Optional for identifing the region of the Aliyun's Service,The default is zh-hangzhou
	RegionID string `json:"region_id,omitempty"`
}

// AppendRecords adds records to the zone. It returns the records that were added.
func (p *Provider) AppendRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		rid, err := p.addDomainRecord(ctx, ar)
		if err != nil {
			return nil, err
		}
		ar.RecID = rid
		rls = append(rls, ar.LibdnsRecord())
	}
	return rls, nil
}

// DeleteRecords deletes the records from the zone. If a record does not have an ID,
// it will be looked up. It returns the records that were deleted.
func (p *Provider) DeleteRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		if ar.RecID == "" {
			r0, err := p.queryDomainRecord(ctx, ar.Rr, ar.DName, ar.DTyp, ar.DVal)
			if err != nil {
				return nil, err
			}
			ar.RecID = r0.RecID
		}
		_, err := p.delDomainRecord(ctx, ar)
		if err != nil {
			return nil, err
		}
		rls = append(rls, ar.LibdnsRecord())
	}
	return rls, nil
}

// GetRecords lists all the records in the zone.
func (p *Provider) GetRecords(ctx context.Context, zone string) ([]libdns.Record, error) {
	var rls []libdns.Record
	recs, err := p.queryDomainRecords(ctx, zone)
	if err != nil {
		return nil, err
	}
	for _, rec := range recs {
		rls = append(rls, rec.LibdnsRecord())
	}
	return rls, nil
}

// SetRecords sets the records in the zone, either by updating existing records
// or creating new ones. It returns the updated records.
func (p *Provider) SetRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	var err error
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		if ar.RecID == "" {
			r0, err := p.queryDomainRecord(ctx, ar.Rr, ar.DName, ar.DTyp, ar.DVal)
			if err != nil {
				ar.RecID, err = p.addDomainRecord(ctx, ar)
				if err != nil {
					return nil, err
				}
			} else {
				ar.RecID = r0.RecID
			}
		} else {
			_, err = p.setDomainRecord(ctx, ar)
			if err != nil {
				return nil, err
			}
		}
		rls = append(rls, ar.LibdnsRecord())
	}
	return rls, nil
}

func (p *Provider) getClient() error {
	return p.getClientWithZone("")
}

func (p *Provider) getClientWithZone(zone string) error {
	cred := newCredInfo(p.AccKeyID, p.AccKeySecret, p.RegionID)
	return p.client.getAliClient(cred, zone)
}

func (p *Provider) addDomainRecord(ctx context.Context, rc aliDomaRecord) (recID string, err error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClientWithZone(rc.DName)
	if rc.TTL <= 0 {
		rc.TTL = 600
	}
	p.client.aClient.addReqBody("Action", "AddDomainRecord")
	p.client.aClient.addReqBody("DomainName", rc.DName)
	p.client.aClient.addReqBody("RR", rc.Rr)
	p.client.aClient.addReqBody("Type", rc.DTyp)
	p.client.aClient.addReqBody("Value", rc.DVal)
	p.client.aClient.addReqBody("TTL", fmt.Sprintf("%d", rc.TTL))
	rs := aliResult{}
	err = p.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (p *Provider) delDomainRecord(ctx context.Context, rc aliDomaRecord) (recID string, err error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClient()
	p.client.aClient.addReqBody("Action", "DeleteDomainRecord")
	p.client.aClient.addReqBody("RecordId", rc.RecID)
	rs := aliResult{}
	err = p.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (p *Provider) setDomainRecord(ctx context.Context, rc aliDomaRecord) (recID string, err error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClientWithZone(rc.DName)
	if rc.TTL <= 0 {
		rc.TTL = 600
	}
	p.client.aClient.addReqBody("Action", "UpdateDomainRecord")
	p.client.aClient.addReqBody("RecordId", rc.RecID)
	p.client.aClient.addReqBody("RR", rc.Rr)
	p.client.aClient.addReqBody("Type", rc.DTyp)
	p.client.aClient.addReqBody("Value", rc.DVal)
	p.client.aClient.addReqBody("TTL", fmt.Sprintf("%d", rc.TTL))
	rs := aliResult{}
	err = p.doAPIRequest(ctx, &rs)
	recID = rs.RecID
	if err != nil {
		return "", err
	}
	return recID, err
}

func (p *Provider) getDomainRecord(ctx context.Context, recID string) (aliDomaRecord, error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClient()
	p.client.aClient.addReqBody("Action", "DescribeDomainRecordInfo")
	p.client.aClient.addReqBody("RecordId", recID)
	rs := aliResult{}
	err := p.doAPIRequest(ctx, &rs)
	rec := rs.ToDomaRecord()
	if err != nil {
		return aliDomaRecord{}, err
	}
	return rec, err
}

func (p *Provider) queryDomainRecords(ctx context.Context, name string) ([]aliDomaRecord, error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClient()
	p.client.aClient.addReqBody("Action", "DescribeDomainRecords")
	p.client.aClient.addReqBody("DomainName", strings.Trim(name, "."))
	rs := aliResult{}
	err := p.doAPIRequest(ctx, &rs)
	if err != nil {
		return []aliDomaRecord{}, err
	}
	return rs.DRecords.Record, err
}

func (p *Provider) queryDomainRecord(ctx context.Context, rr, name string, recType string, recVal ...string) (aliDomaRecord, error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClient()
	p.client.aClient.addReqBody("Action", "DescribeDomainRecords")
	p.client.aClient.addReqBody("DomainName", strings.Trim(name, "."))
	p.client.aClient.addReqBody("RRKeyWord", rr)
	if recType != "" {
		p.client.aClient.addReqBody("TypeKeyWord", recType)
	}
	if len(recVal) > 0 && recVal[0] != "" {
		p.client.aClient.addReqBody("ValueKeyWord", recVal[0])
	}
	p.client.aClient.addReqBody("SearchMode", "ADVANCED")
	rs := aliResult{}
	err := p.doAPIRequest(ctx, &rs)
	if err != nil {
		return aliDomaRecord{}, err
	}
	if len(rs.DRecords.Record) == 0 {
		return aliDomaRecord{}, errors.New("the Record Name of the domain not found")
	}
	return rs.DRecords.Record[0], err
}

// REVERSED:queryMainDomain rseserved for absolute names to name,zone
func (p *Provider) queryMainDomain(ctx context.Context, name string) (string, string, error) {
	p.client.mutex.Lock()
	defer p.client.mutex.Unlock()
	p.getClient()
	p.client.aClient.addReqBody("Action", "GetMainDomainName")
	p.client.aClient.addReqBody("InputString", strings.Trim(name, "."))
	rs := aliResult{}
	err := p.doAPIRequest(ctx, &rs)
	if err != nil {
		return "", "", err
	}
	return rs.Rr, rs.DName, err
}

func (p *Provider) doAPIRequest(ctx context.Context, result interface{}) error {
	return p.client.doAPIRequest(ctx, "GET", result)
}

// Interface guards
var (
	_ libdns.RecordGetter   = (*Provider)(nil)
	_ libdns.RecordAppender = (*Provider)(nil)
	_ libdns.RecordSetter   = (*Provider)(nil)
	_ libdns.RecordDeleter  = (*Provider)(nil)
)
