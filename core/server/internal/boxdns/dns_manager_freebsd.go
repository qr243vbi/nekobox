//go:build freebsd
package boxdns

import "context"

type dnsManager struct{}

func (m *dnsManager) Start() error                                         { return nil }
func (m *dnsManager) Stop() error                                          { return nil }
func (m *dnsManager) SetServers(ctx context.Context, servers []string) error { return nil }
func (m *dnsManager) GetServers(ctx context.Context) ([]string, error)     { return []string{}, nil }
