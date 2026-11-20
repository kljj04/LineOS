# lwm

[Documentation index](README.md) · [한국어](../ko_KR/Lwm.md) · [English](../en_US/Lwm.md)

Lwm installs at `/sys/lwm/lwm.lix` and starts through [user autostart](Autostart.md). The initial desktop contains an undecorated background shell, not an ordinary shell window or automatic demo window. Lwm waits for Terminal's first frame before covering the screen. If Terminal does not connect within five seconds, lwm exits and returns display control to the CLI path.

## Window controls

Lwm draws borders, title bars, shadows, minimize/maximize/close controls and a bottom dock. Ordinary windows have 12-pixel rounded corners with 4×4 coverage antialiasing; maximized windows have square corners. Applications provide content pixels only.

| Control | Behavior |
| --- | --- |
| Title-bar drag | Move a window |
| Drag within the inner 6 pixels of an edge/corner | Resize; restore a maximized window first |
| Minimized window's dock entry | Restore |
| Maximize button | Fill the area outside the panel/dock; toggle to restore geometry |
| Close button | Send a cooperative close event and remove the window; no forced process termination |
| TestGUI + dock button | Start a separate TestGUI process |
| Dock exit button | Exit lwm; q/Escape inside an application does not exit the compositor |

Terminal's background layer has no title bar, borders or window-control buttons and is excluded from the ordinary window list.

## Focus and input

Terminal and TestGUI run in separate processes. Terminal frames form the bottom layer, with application windows, shadows and dock above. Clicking an uncovered shell area or the Shell dock button gives the CLI keyboard focus; clicking a GUI window focuses that client. Closing or minimizing a window preserves the background shell. TestGUI falls back to its fullscreen path if no server is available.

Terminal continues consuming its master output queue while publishing IPC frames. The kernel separates hardware input read by the GUI owner from input forwarded by Terminal to the shell, preventing the shell and lwm from consuming the same source queue competitively.

The existing shell process, editing state, output and history survive GUI connect/disconnect. Terminal adjusts available height for the dock without recreating the shell or clearing the complete screen model.

## Resizing

GUI client buffers retain their original resolution and are displayed with nearest-neighbor scaling. The background shell keeps native pixel size within the area outside the dock. Pointer coordinates are converted to original client coordinates. Native client relayout, terminal row/column reflow and multiple independent terminal sessions are unsupported.

## Composition and updates

Each window separates committed client frames, incoming frames and decoration buffers. Clients transmit changed rows through PIXELS and commit with PRESENT; partial frames are not displayed. Title/dock glyphs are cached. Changed shell rows, old/new window rectangles, shadows and dock state determine recomposition. Shadows process outlines rather than every interior pixel. Resulting damage rectangles are presented through framebuffer ioctl without repeatedly copying/comparing an unchanged desktop.

Composition is capped at 120 Hz using monotonic time, approximately 8.333 ms per frame. Unchanged scenes are not redrawn. Input is checked separately at intervals up to 1 ms. Terminal tracks changed text rows, old/new cursor and scrollbar regions, comparing/transmitting those rows rather than the full frame per keystroke. These caps are scheduling targets, not measured 120 FPS or input-latency guarantees.

## Limits

| Resource | Limit |
| --- | --- |
| Ordinary windows | 8 |
| IPC client content | Up to 2560×1440 |
| Compositor framebuffer | 640×480 through 4096×2160, contiguous 32-bit RGB rows |
| Shared Terminal framebuffer | Up to 2560×1440 |

Actual throughput depends on CPU composition, IPC, timer precision and GPU presentation. Latency and CPU utilization have not been characterized. See [IPC](IPC.md) for the protocol and [Validation](Validation.md) for test coverage.
