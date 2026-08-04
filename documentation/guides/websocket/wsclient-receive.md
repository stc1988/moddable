---
name: Receive Message using WebSocket Client
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

When the WebSocket Client receives message data, it invokes `onReadable()`. The message may be an incomplete fragment, even if the message size is small. Therefore, `onReadable()` must handle fragmented message delivery. The options object's `more` is `true` for all fragments except the last. Its `binary` property indicates whether the message is text or binary data.

This implementation of `onReadable()` uses a resizable `ArrayBuffer` to reassemble message fragments, logging them to console once the entire message is received.

```js
const wsc = new device.network.ws.io({
	...device.network.ws,
	host: "example.com",
	path: "/ws",
	onReadable(count, options) {
		let bytes = new Uint8Array(this.read(count));

		if (this.fragments || options.more) {
			this.fragments ??= new Uint8Array(
				new ArrayBuffer(0, {maxByteLength: 1024 * 1024}));
			this.fragments.buffer.resize(this.fragments.length + count);
			this.fragments.set(bytes, this.fragments.length - count);

			if (options.more)
				return;

			bytes = this.fragments;
			delete this.fragments;
		}

		if (options.binary)
			trace(`binary: ${bytes.toHex()}\n`);
		else
			trace(`string: ${String.fromArrayBuffer(bytes.buffer)}\n`);
	}
});
```
