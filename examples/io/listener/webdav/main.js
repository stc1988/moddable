/*
 * Copyright (c) 2016-2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 *
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import WebDAV from "embedded:network/http/server/route/webdav";
import StaticRoute from "embedded:network/http/server/route/static";
import files from "embedded:storage/files";
import Net from "net";

// serve the "dav" directory in the files root
files.createDirectory("dav");
const dav = new WebDAV({
	directory: files.openDirectory({path: "dav"}),
	prefix: "/dav",
	log: false,
	appleDouble: false		// refuse "._" metadata files - macOS Finder copies with far fewer requests, but Finder metadata is not preserved
});

const port = 8080;
const url = `http://${Net.get("IP")}:${port}${dav.prefix}/`;

new device.network.http.server.io({
	...device.network.http.server,
	port,
	onRoute(request) {
		const route = dav.onRoute(request);
		if (route)
			return route;

		// WebDAV composes with other routes
		if (("GET" === request.method) && ("/" === request.path)) {
			return {
				...StaticRoute,
				contentType: "text/html",
				data: `<!DOCTYPE html><html><head><meta charset="utf-8"><title>WebDAV server</title></head><body>
							<p>WebDAV server ready at <a href="${url}">${url}</a>.
							Mount with a WebDAV client or open in a web browser.</p>
							</body></html>`
			};
		}
	}
});

trace(`WebDAV server ready at ${url}. Mount with a WebDAV client or open in a web browser\n`);
