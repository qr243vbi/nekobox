package cloudflare

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/url"
	"strings"

	"github.com/libdns/libdns"
)

func (p *Provider) createRecord(ctx context.Context, zoneInfo cfZone, record libdns.Record) (cfDNSRecord, error) {
	cfRec, err := cloudflareRecord(record)
	if err != nil {
		return cfDNSRecord{}, err
	}
	jsonBytes, err := json.Marshal(cfRec)
	if err != nil {
		return cfDNSRecord{}, err
	}

	reqURL := fmt.Sprintf("%s/zones/%s/dns_records", baseURL, zoneInfo.ID)
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, reqURL, bytes.NewReader(jsonBytes))
	if err != nil {
		return cfDNSRecord{}, err
	}
	req.Header.Set("Content-Type", "application/json")

	var result cfDNSRecord
	_, err = p.doAPIRequest(req, &result)
	if err != nil {
		return cfDNSRecord{}, err
	}

	return result, nil
}

// updateRecord updates a DNS record. oldRec must have both an ID and zone ID.
// Only the non-empty fields in newRec will be changed.
func (p *Provider) updateRecord(ctx context.Context, oldRec, newRec cfDNSRecord) (cfDNSRecord, error) {
	reqURL := fmt.Sprintf("%s/zones/%s/dns_records/%s", baseURL, oldRec.ZoneID, oldRec.ID)
	jsonBytes, err := json.Marshal(newRec)
	if err != nil {
		return cfDNSRecord{}, err
	}

	// PATCH changes only the populated fields; PUT resets Type, Name, Content, and TTL even if empty
	req, err := http.NewRequestWithContext(ctx, http.MethodPatch, reqURL, bytes.NewReader(jsonBytes))
	if err != nil {
		return cfDNSRecord{}, err
	}
	req.Header.Set("Content-Type", "application/json")

	var result cfDNSRecord
	_, err = p.doAPIRequest(req, &result)
	return result, err
}

func (p *Provider) getDNSRecords(ctx context.Context, zoneInfo cfZone, rec libdns.Record, matchContent bool) ([]cfDNSRecord, error) {
	rr, err := cloudflareRecord(rec)
	if err != nil {
		return nil, err
	}

	qs := make(url.Values)
	qs.Set("type", rr.Type)
	qs.Set("name", libdns.AbsoluteName(rr.Name, zoneInfo.Name))

	var unwrappedContent string
	if matchContent {
		if rr.Type == "TXT" {
			unwrappedContent = unwrapContent(rr.Content)
			// Use the contains (wildcard) search with unquoted content to return both quoted and unquoted content
			qs.Set("content.contains", unwrappedContent)
		} else {
			qs.Set("content.exact", rr.Content)
		}
	}

	reqURL := fmt.Sprintf("%s/zones/%s/dns_records?%s", baseURL, zoneInfo.ID, qs.Encode())
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, reqURL, nil)
	if err != nil {
		return nil, err
	}

	var results []cfDNSRecord
	_, err = p.doAPIRequest(req, &results)

	// Since the TXT search used contains (wildcard), check for exact matches
	if matchContent && rr.Type == "TXT" {
		for i := 0; i < len(results); i++ {
			// Prefer exact quoted content
			if results[i].Content == rr.Content {
				return []cfDNSRecord{results[i]}, nil
			}
		}

		for i := 0; i < len(results); i++ {
			// Using exact unquoted content is acceptable
			if results[i].Content == unwrappedContent {
				return []cfDNSRecord{results[i]}, nil
			}
		}

		return []cfDNSRecord{}, nil
	}

	return results, err
}

func (p *Provider) getZoneInfo(ctx context.Context, zoneName string) (cfZone, error) {
	p.zonesMu.Lock()
	defer p.zonesMu.Unlock()

	// if we already got the zone info, reuse it
	if p.zones == nil {
		p.zones = make(map[string]cfZone)
	}
	if zone, ok := p.zones[zoneName]; ok {
		return zone, nil
	}

	qs := make(url.Values)
	qs.Set("name", zoneName)
	reqURL := fmt.Sprintf("%s/zones?%s", baseURL, qs.Encode())

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, reqURL, nil)
	if err != nil {
		return cfZone{}, err
	}

	if p.ZoneToken != "" {
		req.Header.Set("Authorization", "Bearer "+p.ZoneToken)
	}
	var zones []cfZone
	_, err = p.doAPIRequest(req, &zones)
	if err != nil {
		return cfZone{}, err
	}
	if len(zones) != 1 {
		return cfZone{}, fmt.Errorf("expected 1 zone, got %d for %s", len(zones), zoneName)
	}

	// cache this zone for possible reuse
	p.zones[zoneName] = zones[0]

	return zones[0], nil
}

// getClient returns http client to use
func (p *Provider) getClient() HTTPClient {
	if p.HTTPClient == nil {
		return http.DefaultClient
	}
	return p.HTTPClient
}

// doAPIRequest does the round trip, adding Authorization header if not already supplied.
// It returns the decoded response from Cloudflare if successful; otherwise it returns an
// error including error information from the API if applicable. If result is a
// non-nil pointer, the result field from the API response will be decoded into
// it for convenience.
func (p *Provider) doAPIRequest(req *http.Request, result any) (cfResponse, error) {
	if req.Header.Get("Authorization") == "" {
		req.Header.Set("Authorization", "Bearer "+p.APIToken)
	}

	resp, err := p.getClient().Do(req)
	if err != nil {
		return cfResponse{}, err
	}
	defer resp.Body.Close()

	var respData cfResponse
	err = json.NewDecoder(resp.Body).Decode(&respData)
	if err != nil {
		return cfResponse{}, err
	}

	if resp.StatusCode >= 400 {
		return cfResponse{}, fmt.Errorf("got error status: HTTP %d: %+v", resp.StatusCode, respData.Errors)
	}
	if len(respData.Errors) > 0 {
		return cfResponse{}, fmt.Errorf("got errors: HTTP %d: %+v", resp.StatusCode, respData.Errors)
	}

	if len(respData.Result) > 0 && result != nil {
		err = json.Unmarshal(respData.Result, result)
		if err != nil {
			return cfResponse{}, err
		}
		respData.Result = nil
	}

	return respData, err
}

const baseURL = "https://api.cloudflare.com/client/v4"

func unwrapContent(content string) string {
	if strings.HasPrefix(content, `"`) && strings.HasSuffix(content, `"`) {
		content = strings.TrimPrefix(strings.TrimSuffix(content, `"`), `"`)
	}
	return content
}

func wrapContent(content string) string {
	if !strings.HasPrefix(content, `"`) && !strings.HasSuffix(content, `"`) {
		content = fmt.Sprintf("%q", content)
	}
	return content
}
