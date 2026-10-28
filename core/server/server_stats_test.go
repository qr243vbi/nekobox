//go:build !darwin

package main

import (
	"nekobox_core/internal/boxbox"
	"sync/atomic"
	"testing"

	"github.com/gofrs/uuid/v5"
	"github.com/sagernet/sing-box/common/trafficcontrol"
)

func trackerFor(id uuid.UUID, outbound string, chain []string, up, down int64) *trafficcontrol.TrackerMetadata {
	upload, download := new(atomic.Int64), new(atomic.Int64)
	upload.Store(up)
	download.Store(down)
	return &trafficcontrol.TrackerMetadata{ID: id, Outbound: outbound, Chain: chain, Upload: upload, Download: download}
}

func collectOnce(c *trafficCounters, box *boxbox.Box, open, closed []*trafficcontrol.TrackerMetadata) (map[string]int64, map[string]int64) {
	ups, downs := map[string]int64{}, map[string]int64{}
	c.collectFrom(box, open, closed, ups, downs)
	return ups, downs
}

func TestTrafficCountersReportDeltasSinceLastCall(t *testing.T) {
	var c trafficCounters
	box := new(boxbox.Box)
	id := uuid.Must(uuid.NewV4())
	conn := trackerFor(id, "proxy", []string{"proxy"}, 100, 1000)

	ups, downs := collectOnce(&c, box, []*trafficcontrol.TrackerMetadata{conn}, nil)
	if ups["proxy"] != 100 || downs["proxy"] != 1000 {
		t.Fatalf("first call = %d/%d, want 100/1000", ups["proxy"], downs["proxy"])
	}

	conn.Upload.Add(5)
	conn.Download.Add(50)
	ups, downs = collectOnce(&c, box, []*trafficcontrol.TrackerMetadata{conn}, nil)
	if ups["proxy"] != 5 || downs["proxy"] != 50 {
		t.Fatalf("second call = %d/%d, want 5/50", ups["proxy"], downs["proxy"])
	}

	ups, _ = collectOnce(&c, box, []*trafficcontrol.TrackerMetadata{conn}, nil)
	if _, ok := ups["proxy"]; ok {
		t.Fatalf("idle call reported %d bytes, want none", ups["proxy"])
	}
}

func TestTrafficCountersCountConnectionClosingBetweenListingsOnce(t *testing.T) {
	var c trafficCounters
	box := new(boxbox.Box)
	id := uuid.Must(uuid.NewV4())
	collectOnce(&c, box, []*trafficcontrol.TrackerMetadata{trackerFor(id, "proxy", nil, 10, 10)}, nil)

	// The connection moved to the closed list after the open listing was taken,
	// and sent a little more on the way out.
	open := trackerFor(id, "proxy", nil, 20, 20)
	closed := trackerFor(id, "proxy", nil, 25, 30)
	ups, downs := collectOnce(&c, box, []*trafficcontrol.TrackerMetadata{open}, []*trafficcontrol.TrackerMetadata{closed})
	if ups["proxy"] != 15 || downs["proxy"] != 20 {
		t.Fatalf("got %d/%d, want 15/20", ups["proxy"], downs["proxy"])
	}

	ups, _ = collectOnce(&c, box, nil, []*trafficcontrol.TrackerMetadata{closed})
	if _, ok := ups["proxy"]; ok {
		t.Fatalf("closed connection counted again: %d", ups["proxy"])
	}
}

func TestTrafficCountersAttributeToEachChainTagOnce(t *testing.T) {
	var c trafficCounters
	conn := trackerFor(uuid.Must(uuid.NewV4()), "wiki00", []string{"select", "wiki00"}, 7, 9)
	ups, downs := collectOnce(&c, new(boxbox.Box), []*trafficcontrol.TrackerMetadata{conn}, nil)
	if ups["select"] != 7 || ups["wiki00"] != 7 || downs["select"] != 9 || downs["wiki00"] != 9 || len(ups) != 2 {
		t.Fatalf("ups=%v downs=%v, want 7/9 for select and wiki00 only", ups, downs)
	}
}

func TestTrafficCountersResetForNewBox(t *testing.T) {
	var c trafficCounters
	id := uuid.Must(uuid.NewV4())
	collectOnce(&c, new(boxbox.Box), []*trafficcontrol.TrackerMetadata{trackerFor(id, "proxy", nil, 500, 500)}, nil)

	// A restarted core starts its counters from zero again.
	ups, _ := collectOnce(&c, new(boxbox.Box), []*trafficcontrol.TrackerMetadata{trackerFor(id, "proxy", nil, 3, 3)}, nil)
	if ups["proxy"] != 3 {
		t.Fatalf("after restart got %d, want 3", ups["proxy"])
	}
}
