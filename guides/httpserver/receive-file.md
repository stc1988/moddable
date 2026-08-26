---
name: Receive Request Body to File
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-25
---

You can map PUT requests to files to upload files to the device's file system. This example maps the HTTP server root to the file system's `./site/` directory.

See the [Files Guide](../files/index.md) for more on files and [Receive Request Body](./receive-body.md) for more on HTTP request bodies.

```js
const root = "site";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("PUT" !== request.method)
			return;
		
		const status = device.files.status(root + request.path);
		if (!status.isDirectory())
			return fileUploadRoute;
	}
});

const fileUploadRoute = {
	onRequest(request) {
		this.file = device.files.openFile({
			path: root + request.path,
			mode: "w"
		});
		this.position = 0;
	},
	onReadable(count) {
		const buffer = this.read(count);
		this.file.write(buffer, this.position);
		this.position += count;
	},
	onResponse(response) {
		response.headers.set("content-length", 0);
		this.respond(response);
	},
	onDone() {
		this.file?.close();
	},
	onError() {
		this.file?.close();
	}
};
```
