---
name: Respond to Request with File
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-25
---

You can map GET requests to files to create a simple web server. This example maps the HTTP server root to the file system's `./site/` directory.

See the [Files Guide](../files/index.md) for more on files and [Respond to Request](./respond.md) for more on HTTP responses.

```js
const root = "site";

const server = new device.network.http.server.io({
	...device.network.http.server,
	onRoute(request) {
		if ("GET" !== request.method)
			return;
		
		const status = device.files.status(root + request.path);
		if (status.isFile())
			return fileDownloadRoute;
	}
});

const fileDownloadRoute = {
	onRequest(request) {
		this.path = root + request.path;
	},
	onResponse(response) {
		this.file = device.files.openFile({
			path: this.path,
			mode: "r"
		});
		const status = this.file.status();
		response.headers.set("content-length", status.size);
		response.headers.set("content-type", getMIME(this.path));
		this.position = 0;
		this.respond(response);
	},
	onWritable(count) {
		const buffer = this.file.read(count, this.position);
		this.position += count;
		this.write(buffer);
	},
	onDone() {
		this.file?.close();
	},
	onError() {
		this.file?.close();
	}
};

function getMIME(path) {
	if (path.endsWith(".html"))
		return "text/html";
	return "application/octet-stream";
}
```
