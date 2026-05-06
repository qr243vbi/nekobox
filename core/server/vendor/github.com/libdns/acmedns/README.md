Joohoi's ACME-DNS provider for [`libdns`](https://github.com/libdns/libdns)
=======================

[![Go Reference](https://pkg.go.dev/badge/test.svg)](https://pkg.go.dev/github.com/libdns/acmedns)

This package implements the [libdns interfaces](https://github.com/libdns/libdns) for [Joohoi's ACME-DNS](https://github.com/joohoi/acme-dns).

ACME-DNS server is meant to be used solely for obtaining HTTPS certificates using [DNS-01 challenges](https://letsencrypt.org/docs/challenge-types/). Its API is by design limited - the only operation ACME-DNS allows is updating TXT records of one subdomain associated with ACME-DNS account. There are at most two records and older records are deleted as new ones are appended. Due to these limitations, this `libdns` provider implements only `RecordAppender` and `RecordDeleted` interfaces. And `DeleteRecords` method is a no-op - it doesn't do anything.

This provider is written mostly for Caddy's `acmedns` plugin. For more information, see:

1. [github.com/caddy-dns/acmedns](https://github.com/caddy-dns/acmedns/)

2. [github.com/joohoi/acme-dns](https://github.com/joohoi/acme-dns)

3. [A Technical Deep Dive: Securing the Automation of ACME DNS Challenge Validation](https://www.eff.org/deeplinks/2018/02/technical-deep-dive-securing-automation-acme-dns-challenge-validation)