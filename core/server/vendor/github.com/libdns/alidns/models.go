package alidns

import (
	"fmt"
	"strings"
	"time"

	"github.com/libdns/libdns"
)

type ttl_t = uint32

type aliDomaRecord struct {
	Rr       string `json:"RR,omitempty"`
	Line     string `json:"Line,omitempty"`
	Status   string `json:"Status,omitempty"`
	Locked   bool   `json:"Locked,omitempty"`
	DTyp     string `json:"Type,omitempty"`
	DName    string `json:"DomainName,omitempty"`
	DVal     string `json:"Value,omitempty"`
	RecID    string `json:"RecordId,omitempty"`
	TTL      ttl_t  `json:"TTL,omitempty"`
	Weight   int    `json:"Weight,omitempty"`
	Priority ttl_t  `json:"Priority,omitempty"`
}

func (r aliDomaRecord) Equals(v aliDomaRecord) bool {
	result := v.Rr == r.Rr
	result = result && v.DName == r.DName
	result = result && v.DVal == r.DVal
	result = result && v.DTyp == r.DTyp
	return result
}

type aliDomaRecords struct {
	Record []aliDomaRecord `json:"Record,omitempty"`
}

type aliResult struct {
	ReqID      string         `json:"RequestId,omitempty"`
	DRecords   aliDomaRecords `json:"DomainRecords,omitempty"`
	DLvl       int            `json:"DomainLevel,omitempty"`
	DVal       string         `json:"Value,omitempty"`
	TTL        ttl_t          `json:"TTL,omitempty"`
	DName      string         `json:"DomainName,omitempty"`
	Rr         string         `json:"RR,omitempty"`
	Msg        string         `json:"Message,omitempty"`
	Rcmd       string         `json:"Recommend,omitempty"`
	HostID     string         `json:"HostId,omitempty"`
	Code       string         `json:"Code,omitempty"`
	TotalCount int            `json:"TotalCount,omitempty"`
	PgSize     int            `json:"PageSize,omitempty"`
	PgNum      int            `json:"PageNumber,omitempty"`
	DTyp       string         `json:"Type,omitempty"`
	RecID      string         `json:"RecordId,omitempty"`
	Line       string         `json:"Line,omitempty"`
	Status     string         `json:"Status,omitempty"`
	Locked     bool           `json:"Locked,omitempty"`
	Weight     int            `json:"Weight,omitempty"`
	MinTTL     int            `json:"MinTtl,omitempty"`
	Priority   ttl_t          `json:"Priority,omitempty"`
}

func (r *aliDomaRecord) LibdnsRecord() libdns.Record {
	return libdns.RR{
		Type: r.DTyp,
		Name: r.Rr,
		Data: r.DVal,
		TTL:  time.Duration(r.TTL) * time.Second,
	}
}

func (r *aliResult) ToDomaRecord() aliDomaRecord {
	return aliDomaRecord{
		RecID:    r.RecID,
		DTyp:     r.DTyp,
		Rr:       r.Rr,
		DName:    r.DName,
		DVal:     r.DVal,
		TTL:      r.TTL,
		Line:     r.Line,
		Status:   r.Status,
		Locked:   r.Locked,
		Weight:   r.Weight,
		Priority: r.Priority,
	}
}

// AlidnsRecord convert libdns.Record with zone to aliDomaRecord
func alidnsRecord(r libdns.Record, zone ...string) aliDomaRecord {
	result := aliDomaRecord{}
	if r == nil {
		return result
	}
	tmpRR := r.RR()
	if len(zone) > 0 && len(zone[0]) > 0 {
		tmpZone := zone[0]
		result.Rr = libdns.RelativeName(tmpRR.Name, tmpZone)
		result.DName = strings.Trim(tmpZone, ".")
	} else {
		result.Rr = tmpRR.Name
	}
	result.DTyp = tmpRR.Type
	result.DVal = tmpRR.Data
	result.TTL = ttl_t(tmpRR.TTL.Seconds())
	if svcb, svcbok := r.(libdns.ServiceBinding); svcbok {
		result.Priority = ttl_t(svcb.Priority)
		result.DVal = fmt.Sprintf("%s %s",svcb.Target,svcb.Params)
	}
	return result
}
