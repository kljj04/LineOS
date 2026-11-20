# IPC and window protocol

[Documentation index](README.md) · [한국어](../ko_KR/IPC.md) · [English](../en_US/IPC.md)

Syscall 1036 provides copied-message endpoints. The shared ABI is Common/Syscall/IPC/IPC.h; the implementation is under Kernel/Source/Syscall/IPC and Kernel/Include/Syscall/IPC. The LOSLib client interface is `<IPC.h>`.

## Ownership and routing

Only an endpoint's owning task may receive, bind or close it. Name lookup is separated by real UID. Ordinary sends are allowed within a UID and from root; explicitly binding to a peer permits replies from that peer. This supports a limited reply relationship between user lwm and root Terminal.

Terminal obtains the registered compositor endpoint through the master TTY ioctl for the current graphics owner rather than arbitrary name lookup. Endpoint IDs increase without reuse. Fork does not grant ownership of the parent's endpoints; successful exec, exit and task destruction clean them up.

## API

Declare LOSLib in Program.cfg Libraries. The following excerpt shows the call sequence; check each result in application code.

```c
INT64 Endpoint = LOSIPCCreate(NULL);
INT64 Server = LOSIPCLookup("lwm");
LOSIPCBind(Endpoint, Server);
LOSIPCSend(Endpoint, Server, Data, Size);
LOSIPCReceive(Endpoint, Buffer, Capacity, &Sender);
LOSIPCClose(Endpoint);
```

## Capacity and errors

| Resource | Limit |
| --- | --- |
| Global endpoints | 64 |
| Messages per receive queue | 16 |
| Message bytes | 64 KiB |
| Total queued payload | 16 MiB |

Empty receive queues and full send queues return EAGAIN. A small receive buffer returns EMSGSIZE without consuming the message. Payload bytes are copied through checked user memory; cross-process pointers are not directly dereferenced.

## Window protocol

Common/Window/Protocol.h defines the protocol; `<Window.h>` belongs to the Window library. Declare Window in Program.cfg; its Library.cfg supplies its LOSLib dependency. The server name is lwm.

```c
LOS_WINDOW Window;
LOSWindowOpen(&Window, 0, "My app", Width, Height, 0);
LOSWindowPresent(&Window, Pixels);
LOSWindowPoll(&Window, &Event);
LOSWindowClose(&Window);
```

The protocol uses CREATE/CREATED, PIXELS/PRESENT, KEY/POINTER and CLOSE/DESTROY. Pixels travel in row chunks, committed by PRESENT. KEY carries input bytes; POINTER uses the client's original content coordinates with button/wheel state. Clients must poll events. CLOSE is cooperative and does not forcibly terminate a process.

Terminal registers with LOS_WINDOW_BACKGROUND. Lwm permits this flag only for a real-UID-0 service and keeps one background layer outside the ordinary window list. Background coordinates do not include decoration offsets. LOSWindowPresentRows compares/transmits caller-supplied changed row ranges so Terminal need not compare an entire frame for every input update.

## Limits

Shared-memory transport, socket APIs, blocking poll, multiple independent terminal PTYs and forced application termination are unsupported. See [lwm](Lwm.md) for composition/input behavior and [Validation](Validation.md) for test scope.
