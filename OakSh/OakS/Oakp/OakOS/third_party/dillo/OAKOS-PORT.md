Dillo source imported from commit 6856c5016de8cc15d077ec8d785bac1a38b4a813.
This source is a real graphical browser, licensed under GPL-3.0-or-later.

The source is intentionally kept as the port reference. `make dillo-port` in
the OakOS directory builds the upstream Linux/FLTK target on the host; it does
not place a hosted executable in the OakOS ISO.

The OakOS backend is not yet implemented. The remaining integration work is:

- use the initial freestanding C++ ABI hooks now present in the kernel; `new`
	uses OakOS `kmalloc`, while `delete` remains a no-op until `kfree` exists;
- use the initial `acorn/dillo_platform.h` bridge for framebuffer availability,
	dimensions, clearing, rectangles and text; window management, fonts and
	input dispatch still need to be implemented; the bridge now also
	exposes frame boundaries, a clipping rectangle and a bounded FIFO for
	keyboard/mouse events, validated during OakOS boot;
- provide the POSIX/libc process, file, socket and time calls used by Dillo;
- connect Dillo's network I/O to the OakOS socket stack and complete the TCP/IP
	implementation; TCP state and local ports are now stored per socket, frames
	are routed by endpoint during polling, gateway ARP is performed during
	connect and FIN is sent during close; data and FIN segments have bounded
	retransmission with per-connection exponential backoff and ACK processing;
	remote FIN is acknowledged; full congestion control and larger connection
	queues are still pending;
- adapt the FLTK window/event and drawing interfaces to the OakOS framebuffer;
- package the resulting user executable only after those interfaces have real
	implementations and an end-to-end QEMU test.

Until then, the OakOS desktop must not present Dillo as an available browser.
