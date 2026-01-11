package cloudflare

import (
	"context"
	"fmt"
	"net/http"
	"net/url"
	"sync"

	"github.com/libdns/libdns"
)

type HTTPClient interface {
	Do(req *http.Request) (*http.Response, error)
}

// Provider implements the libdns interfaces for Cloudflare.
// TODO: Support retries and handle rate limits.
type Provider struct {
	// API tokens are used for authentication. Make sure to use
	// scoped API **tokens**, NOT a global API **key**.
	APIToken  string `json:"api_token,omitempty"`  // API token with Zone.DNS:Write (can be scoped to single Zone if ZoneToken is also provided)
	ZoneToken string `json:"zone_token,omitempty"` // Optional Zone:Read token (global scope)

	// HTTPClient is the client used to communicate with Cloudflare.
	// If nil, a default client will be used.
	HTTPClient HTTPClient `json:"-"`

	zones   map[string]cfZone
	zonesMu sync.Mutex
}

// GetRecords lists all the records in the zone.
func (p *Provider) GetRecords(ctx context.Context, zone string) ([]libdns.Record, error) {
	zoneInfo, err := p.getZoneInfo(ctx, zone)
	if err != nil {
		return nil, err
	}

	page := 1
	const maxPageSize = 100

	var allRecords []cfDNSRecord
	for {
		qs := make(url.Values)
		qs.Set("page", fmt.Sprintf("%d", page))
		qs.Set("per_page", fmt.Sprintf("%d", maxPageSize))
		reqURL := fmt.Sprintf("%s/zones/%s/dns_records?%s", baseURL, zoneInfo.ID, qs.Encode())

		req, err := http.NewRequestWithContext(ctx, http.MethodGet, reqURL, nil)
		if err != nil {
			return nil, err
		}

		var pageRecords []cfDNSRecord
		response, err := p.doAPIRequest(req, &pageRecords)
		if err != nil {
			return nil, err
		}

		allRecords = append(allRecords, pageRecords...)

		lastPage := (response.ResultInfo.TotalCount + response.ResultInfo.PerPage - 1) / response.ResultInfo.PerPage
		if response.ResultInfo == nil || page >= lastPage || len(pageRecords) == 0 {
			break
		}

		page++
	}

	recs := make([]libdns.Record, 0, len(allRecords))
	for _, rec := range allRecords {
		libdnsRec, err := rec.libdnsRecord(zone)
		if err != nil {
			return nil, fmt.Errorf("parsing Cloudflare DNS record %+v: %v", rec, err)
		}
		recs = append(recs, libdnsRec)
	}

	return recs, nil
}

// AppendRecords adds records to the zone. It returns the records that were added.
func (p *Provider) AppendRecords(ctx context.Context, zone string, records []libdns.Record) ([]libdns.Record, error) {
	zoneInfo, err := p.getZoneInfo(ctx, zone)
	if err != nil {
		return nil, err
	}

	var created []libdns.Record
	for _, rec := range records {
		result, err := p.createRecord(ctx, zoneInfo, rec)
		if err != nil {
			return nil, err
		}
		libdnsRec, err := result.libdnsRecord(zone)
		if err != nil {
			return nil, fmt.Errorf("parsing Cloudflare DNS record %+v: %v", rec, err)
		}
		created = append(created, libdnsRec)
	}

	return created, nil
}

// DeleteRecords deletes the records from the zone. If a record does not have an ID,
// it will be looked up. It returns the records that were deleted.
func (p *Provider) DeleteRecords(ctx context.Context, zone string, records []libdns.Record) ([]libdns.Record, error) {
	zoneInfo, err := p.getZoneInfo(ctx, zone)
	if err != nil {
		return nil, err
	}

	var recs []libdns.Record
	for _, rec := range records {
		// record ID is required; try to find it with what was provided
		exactMatches, err := p.getDNSRecords(ctx, zoneInfo, rec, true)
		if err != nil {
			return nil, err
		}

		for _, cfRec := range exactMatches {
			reqURL := fmt.Sprintf("%s/zones/%s/dns_records/%s", baseURL, zoneInfo.ID, cfRec.ID)
			req, err := http.NewRequestWithContext(ctx, "DELETE", reqURL, nil)
			if err != nil {
				return nil, err
			}

			var result cfDNSRecord
			_, err = p.doAPIRequest(req, &result)
			if err != nil {
				return nil, err
			}

			libdnsRec, err := result.libdnsRecord(zone)
			if err != nil {
				return nil, fmt.Errorf("parsing Cloudflare DNS record %+v: %v", rec, err)
			}
			recs = append(recs, libdnsRec)
		}

	}

	return recs, nil
}

// SetRecords sets the records in the zone, either by updating existing records
// or creating new ones. It returns the updated records.
func (p *Provider) SetRecords(ctx context.Context, zone string, records []libdns.Record) ([]libdns.Record, error) {
	zoneInfo, err := p.getZoneInfo(ctx, zone)
	if err != nil {
		return nil, err
	}

	var results []libdns.Record
	for _, rec := range records {
		oldRec, err := cloudflareRecord(rec)
		if err != nil {
			return nil, err
		}
		oldRec.ZoneID = zoneInfo.ID

		// the record might already exist, even if we don't know the ID yet
		matches, err := p.getDNSRecords(ctx, zoneInfo, rec, false)
		if err != nil {
			return nil, err
		}
		if len(matches) == 0 {
			// record doesn't exist; create it
			result, err := p.createRecord(ctx, zoneInfo, rec)
			if err != nil {
				return nil, err
			}
			libdnsRec, err := result.libdnsRecord(zone)
			if err != nil {
				return nil, fmt.Errorf("parsing Cloudflare DNS record %+v: %v", rec, err)
			}
			results = append(results, libdnsRec)
			continue
		}
		if len(matches) > 1 {
			return nil, fmt.Errorf("unexpectedly found more than 1 record for %v", rec)
		}
		// record does exist, fill in the ID so that we can update it
		oldRec.ID = matches[0].ID

		// record exists; update it
		cfRec, err := cloudflareRecord(rec)
		if err != nil {
			return nil, err
		}
		result, err := p.updateRecord(ctx, oldRec, cfRec)
		if err != nil {
			return nil, err
		}
		libdnsRec, err := result.libdnsRecord(zone)
		if err != nil {
			return nil, fmt.Errorf("parsing Cloudflare DNS record %+v: %v", rec, err)
		}
		results = append(results, libdnsRec)
	}

	return results, nil
}

// Interface guards
var (
	_ libdns.RecordGetter   = (*Provider)(nil)
	_ libdns.RecordAppender = (*Provider)(nil)
	_ libdns.RecordSetter   = (*Provider)(nil)
	_ libdns.RecordDeleter  = (*Provider)(nil)
)
