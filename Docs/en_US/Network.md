# Networking

[Documentation index](README.md) · [한국어](../ko_KR/Network.md) · [English](../en_US/Network.md)

## Connection and scope

The default QEMU arguments are `-netdev user,id=net0` and `-device virtio-net-pci,netdev=net0,mac=52:54:00:4c:4f:53`. QEMU user NAT requires no host bridge or TAP. Inbound connections need explicit port forwarding. The existing libvirt `virbr0` is unrelated to this launcher.

The kernel implements VirtIO-Net, Ethernet, ARP, IPv4, ICMP, UDP, DNS A queries, DHCPv4 and a TCP client. HTTP and the web renderer have been removed. HTTP/HTTPS/TLS are intended for future user space.

## Configuration and worker

`NetworkTask` handles receive processing and DHCP lease renewal through NetworkPoll and a 1 ms timed sleep. It is not registered by the current KMain; `NetworkInit()` initializes the core without launching a task or obtaining a DHCP lease by itself. Input Wait Queues do not change this network worker into interrupt-driven blocking. DHCP obtains the address, subnet mask, gateway and DNS. `NetworkConfigure()` applies static configuration and disables DHCP. `NetworkGetConfig()` reports whether configuration is ready and returns its values.

## UDP

Up to 8 sockets, each with an 8-datagram receive queue, are available through `NetworkUDPOpen`, `NetworkUDPSendTo`, `NetworkUDPReceiveFrom` and `NetworkUDPClose`. Receives preserve source address and port. A short buffer reports the required length and retains the datagram. `NetworkUDPExchange` is a request/reply convenience API.

## TCP

Up to 16 client connections are available through socket handles. Each connection owns its local port, sequence/window state, retransmission state and 8 KiB receive buffer. Packets are dispatched by peer address, peer port and local port, including connections to the same server endpoint.

| API | Contract |
| --- | --- |
| `NetworkTCPConnect(Address, Port, &Socket, TimeoutMls)` | Returns a handle after handshake; writes `UINT32_MAX` on failure |
| `NetworkTCPSend(Socket, Buffer, Length, TimeoutMls)` | Sends on the selected connection |
| `NetworkTCPReceive(Socket, Buffer, Capacity, &Length, TimeoutMls)` | Receives on the selected connection; successful zero length means orderly EOF |
| `NetworkTCPClose(Socket, TimeoutMls)` | Gracefully closes after pending reads/writes finish and unread data is drained |
| `NetworkTCPAbort(Socket)` | Cancels the selected connection and invalidates its handle |

This replaces the earlier implicit global connection API. Callers must retain the returned handle and supply it on each operation. Generations reject stale handles when a slot is reused. Separate connections can progress concurrently; one reader and one writer can run concurrently on the same connection. A second reader or writer, or close during an active operation, returns a busy failure. Abort may interrupt an operation; the slot remains unavailable for reuse until that operation releases ownership.

Retransmission, sequence/duplicate validation, partial ACKs, receive windows and zero-window probes remain supported. A send timeout aborts only that connection; receive timeout leaves it open. Address configuration changes invalidate all connections. `NetworkGetLastError()` remains a shared diagnostic, not task-local error storage.

TIME_WAIT retains at most 64 records for 120 seconds, reserving room for active connections. New connects fail when this capacity is exhausted rather than evicting an unexpired record. Failed connects and explicit aborts release their slots.

## Source

Interfaces are in `Kernel/Include/Network/`. Implementations are in `Kernel/Source/Network/`: `VirtIONetwork.c` for the device, `Network.c` for the core, and `UDP.c`, `TCP.c`, `DHCP.c` and `DNS.c` for protocols.

## Limits

TCP listen, SACK, window scaling and full congestion control are not implemented. TCP uses a simple sequential send scheme with MSS 536. IPv6, IPv4 fragment reassembly and DHCP address-conflict detection are also unsupported. NAT does not replace packet validation.

See [clock and NTP](Clock.md) for UDP time synchronization and independent uptime.
