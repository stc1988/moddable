---
name: Send Message using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use `write()` to transmit messages. The WebSocket Client does not buffer outgoing messages, so you cannot write more bytes than reported to `onWritable()`.

Message data is always provided as a [Byte Buffer](https://419.ecma-international.org/#byte-buffer). The default is to transmit them as binary data. To transmit them as text, set `binary` to `false` on the optional options object.

```js
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		this.write(Uint8Array.of(1,2,3));
		this.write(ArrayBuffer.fromString("msg"),
			{binary: false});
	}
});
```

---

Messages may be sent in fragments to support message sizes unconstrained by network buffers and available memory. To send a message in fragments, set `more` to `true` in all fragments except the last. Both text and binary messages may be sent in fragments, but all fragments must be of the same type.

This example sends a binary message in four fragments.

```js
let fragments = 0;
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		if (fragments > 3) return;
		this.write(new ArrayBuffer(count),
			{more: 3 !== fragments});
		fragments += 1;
	}
});
```

---

You can call `write()` any time, not only from `onWritable()`. Because `write()` does not buffer, a good practice is to track the number of bytes that may be written to avoid buffer overflows.

This example sends a message once a second only if there is enough write space available.

Notice that `write()` returns the updated writable count. Sending a four byte message reduces `writable` by more than four bytes because message overhead depends on whether TLS is used and the message size.

```js
import Timer from "timer";

let writable = 0;
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onWritable(count) {
		writable = count;
	}
});

Timer.repeat(() => {
	if (writable >= 4)
		writable = wsc.write(Uint8Array.of(1, 2, 3, 4));
}, 1000);
```
