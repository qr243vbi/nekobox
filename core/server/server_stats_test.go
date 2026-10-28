//go:build !darwin

package main

import (
	"context"
	"testing"

	"nekobox_core/gen"
)

func TestQueryStatsReportsEveryOutbound(t *testing.T) {
	s := &server{}
	out, err := s.Start(context.Background(), &gen.LoadConfigReq{
		CoreConfig: `{"log":{"disabled":true},"experimental":{"clash_api":{}},` +
			`"outbounds":[{"type":"direct","tag":"proxy"},{"type":"direct","tag":"direct"}]}`,
	})
	if err != nil || out.Error != "" {
		t.Fatalf("Start = %v, %q; want success", err, out.Error)
	}
	defer s.Stop(context.Background(), &gen.EmptyReq{})

	// Idle outbounds are reported as zero, so the GUI keeps its rate clock current.
	for i := 0; i < 2; i++ {
		stats, err := s.QueryStats(context.Background(), &gen.EmptyReq{})
		if err != nil {
			t.Fatal(err)
		}
		for _, tag := range []string{"proxy", "direct"} {
			if up, ok := stats.Ups[tag]; !ok || up != 0 || stats.Downs[tag] != 0 {
				t.Fatalf("call %d: %s = %d/%d, present %v; want 0/0, present", i, tag, up, stats.Downs[tag], ok)
			}
		}
	}

	s.Stop(context.Background(), &gen.EmptyReq{})
	stats, err := s.QueryStats(context.Background(), &gen.EmptyReq{})
	if err != nil || len(stats.Ups) != 0 || len(stats.Downs) != 0 {
		t.Fatalf("after Stop: ups=%v downs=%v err=%v; want none", stats.Ups, stats.Downs, err)
	}
}
