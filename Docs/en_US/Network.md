# Networking

[Documentation index](README.md) · [한국어](../ko_KR/Network.md) · [English](../en_US/Network.md)

## Connection and scope

The default QEMU arguments are `-netdev user,id=net0` and `-device virtio-net-pci,netdev=net0,mac=52:54:00:4c:4f:53`. QEMU user NAT requires no host bridge or TAP. Inbound connections need explicit port forwarding. The existing libvirt `virbr0` is unrelated to this launcher.

The kernel implements VirtIO-Net, Ethernet, ARP, IPv4, ICMP, UDP, DNS A queries, DHCPv4 and a TCP client. HTTP and the test web renderer have been removed. HTTP/HTTPS/TLS are intended for future user space.

## Configuration and worker

`NetworkTask` handles receive processing and DHCP lease renewal. DHCP obtains the address, subnet mask, gateway and DNS. `NetworkConfigure()` applies static configuration and disables DHCP. `NetworkGetConfig()` reports whether configuration is ready and returns its values.

## UDP

Up to 8 sockets, each with an 8-datagram receive queue, are available through `NetworkUDPOpen`, `NetworkUDPSendTo`, `NetworkUDPReceiveFrom` and `NetworkUDPClose`. Receives preserve source address and port. A short buffer reports the required length and retains the datagram. `NetworkUDPExchange` is a request/reply convenience API.

## TCP

A single client connection is available through `NetworkTCPConnect`, `NetworkTCPSend`, `NetworkTCPReceive`, `NetworkTCPClose` and `NetworkTCPAbort`. Implemented paths include retransmission, sequence and duplicate validation, partial ACKs, receive windows, zero-window probes and bounded TIME_WAIT retention.

## Source

Interfaces are in `Kernel/Include/Network/`. Implementations are in `Kernel/Source/Network/`: `VirtIONetwork.c` for the device, `Network.c` for the core, and `UDP.c`, `TCP.c`, `DHCP.c` and `DNS.c` for protocols.

## Limits and verification

TCP listen, multiple TCP connections, SACK, window scaling and full congestion control are not implemented. TCP uses a simple sequential send scheme with MSS 536. IPv6, IPv4 fragment reassembly and DHCP address-conflict detection are also unsupported. NAT does not replace packet validation.

The local `Tests/NetworkStack/Run.py --cpus 16 --scheduled` harness exercises DHCP, gateway ICMP, UDP echo, external DNS and a 12,000-byte TCP stream with EOF. Host sanitizer checks cover malformed packets, UDP queues and handles, DHCP lease state and TCP retransmission/close paths. See [verification](Testing.md) for checkpoint scope and local test availability.
