package alidns

import (
	"context"

	"github.com/libdns/libdns"
)

// Provider implements the libdns interfaces for Alicloud.
type Provider struct {
	client *aliClient
	CredentialInfo
}

// AppendRecords adds records to the zone. It returns the records that were added.
func (p *Provider) AppendRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	var errs = OpErrors("AppendRecords")
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		rid, err := p.addDomainRecord(ctx, ar)
		if err != nil {
			errs.JoinRecord(rec, err)
			continue
		}
		ar.RecordID = rid
		rls = append(rls, ar.DomainRecord())
	}
	return rls, errs.Error()
}

// DeleteRecords deletes the records from the zone. If a record does not have an ID,
// it will be looked up. It returns the records that were deleted.
func (p *Provider) DeleteRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	var errs = OpErrors("DeleteRecords")
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		if ar.RecordID == "" {
			r0, err := p.queryDomainRecord(ctx, ar.Rr, ar.DomainName, ar.DomainType, ar.DomainValue)
			if err != nil {
				errs.JoinRecord(rec, err)
				continue
			}
			ar.RecordID = r0.RecordID
		}
		_, err := p.delDomainRecord(ctx, ar)
		if err != nil {
			errs.JoinRecord(rec, err)
			continue
		}
		rls = append(rls, ar.DomainRecord())
	}
	return rls, errs.Error()
}

// GetRecords lists all the records in the zone.
func (p *Provider) GetRecords(ctx context.Context, zone string) ([]libdns.Record, error) {
	var rls []libdns.Record
	recs, err := p.queryDomainRecords(ctx, zone)
	if err != nil {
		return nil, OpError("GetRecords", err)
	}
	for _, rec := range recs {
		rls = append(rls, rec.DomainRecord())
	}
	return rls, nil
}

// SetRecords sets the records in the zone, either by updating existing records
// or creating new ones. It returns the updated records.
func (p *Provider) SetRecords(ctx context.Context, zone string, recs []libdns.Record) ([]libdns.Record, error) {
	var rls []libdns.Record
	var errs = OpErrors("SetRecords")
	for _, rec := range recs {
		ar := alidnsRecord(rec, zone)
		if ar.RecordID == "" {
			r0, err := p.queryDomainRecord(ctx, ar.Rr, ar.DomainName, ar.DomainType, ar.DomainValue)
			if err == nil {
				ar.RecordID = r0.RecordID
			}
			if ar.Rr == r0.Rr && len(ar.RecordID) > 0 {
				_, err := p.delDomainRecord(ctx, ar)
				if err != nil {
					errs.JoinRecord(rec, err)
					continue
				}
				ar.RecordID = ""
			}
			if len(ar.RecordID) == 0 {
				ar.RecordID, err = p.addDomainRecord(ctx, ar)
				if err != nil {
					errs.JoinRecord(rec, err)
				} else {
					rls = append(rls, ar.DomainRecord())
				}
				continue
			}
		}
		_, err := p.setDomainRecord(ctx, ar)
		if err != nil {
			errs.JoinRecord(rec, err)
			continue
		}
		rls = append(rls, ar.DomainRecord())
	}
	return rls, errs.Error()
}

func (p *Provider) getClient() error {
	return p.getClientWithZone("")
}

func (p *Provider) getClientWithZone(zone string) error {
	var err error
	if len(zone) == 0 {
		p.client, err = getClient(&p.CredentialInfo)
	} else {
		p.client, err = getClient(&p.CredentialInfo, zone)
	}
	if err != nil {
		return err
	}
	return nil
}

func (p *Provider) addDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	err = p.getClientWithZone(rc.DomainName)
	if err != nil {
		return "", err
	}
	if !p.client.IsEntprienseEdition() && rc.TTL < 600 {
		rc.TTL = 600
	}
	return p.client.addDomainRecord(ctx, rc)
}

func (p *Provider) delDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	err = p.getClientWithZone(rc.DomainName)
	if err != nil {
		return "", err
	}
	if !p.client.IsEntprienseEdition() && rc.TTL < 600 {
		rc.TTL = 600
	}
	return p.client.delDomainRecord(ctx, rc)
}

func (p *Provider) setDomainRecord(ctx context.Context, rc aliDomainRecord) (recID string, err error) {
	err = p.getClientWithZone(rc.DomainName)
	if err != nil {
		return "", err
	}
	if !p.client.IsEntprienseEdition() && rc.TTL < 600 {
		rc.TTL = 600
	}
	return p.client.setDomainRecord(ctx, rc)
}

func (p *Provider) getDomainRecord(ctx context.Context, recID string) (aliDomainRecord, error) {
	p.getClient()
	return p.client.getDomainRecord(ctx, recID)
}

func (p *Provider) queryDomainRecords(ctx context.Context, name string) ([]aliDomainRecord, error) {
	p.getClient()
	return p.client.queryDomainRecords(ctx, name)
}

func (p *Provider) queryDomainRecord(ctx context.Context, rr, name string, recType string, recVal ...string) (aliDomainRecord, error) {
	p.getClient()
	return p.client.queryDomainRecord(ctx, rr, name, recType, recVal...)
}

// Interface guards
var (
	_ libdns.RecordGetter   = (*Provider)(nil)
	_ libdns.RecordAppender = (*Provider)(nil)
	_ libdns.RecordSetter   = (*Provider)(nil)
	_ libdns.RecordDeleter  = (*Provider)(nil)
)
