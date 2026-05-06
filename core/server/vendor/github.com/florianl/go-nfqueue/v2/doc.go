/*
Package nfqueue provides an API to interact with the nfqueue subsystem of the netfilter family from the linux kernel.

This package processes information directly from the kernel and therefore it requires special privileges. You
can provide this privileges by adjusting the CAP_NET_ADMIN capabilities.

	setcap 'cap_net_admin=+ep' /your/executable
*/
package nfqueue
