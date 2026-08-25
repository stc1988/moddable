/*
 * Copyright (c) 2026  Moddable Tech, Inc.
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

import CaptivePortal from "captiveportal";
import Resource from "Resource";

new CaptivePortal({
	SSID: "Moddable-AP",
	password: "moddable",
	onPage(path) {
		const name = ("/" === path) ? "index.html" : path.slice(1);
		if (!Resource.exists(name)) return;

		const extension = name.slice(name.lastIndexOf(".") + 1).toLowerCase();
		return {content: new Resource(name), mimeType: extensionToMIMEType[extension] ?? "application/octet-stream"};
	},
	onClose() {
		trace(`portal closed\n`);
	},
	onStatus(phase, detail) {
		trace(`portal: ${phase}${detail ? ` (${JSON.stringify(detail)})` : ""}\n`);
	},
	onInfo(msg) {
		trace(`portal: info ${JSON.stringify(msg)}\n`);
	},
	onError(err) {
		trace(`portal error: ${err.message}\n`);
	}
});

const extensionToMIMEType = {
	html: "text/html",
	js: "application/javascript",
	css: "text/css",
	json: "application/json",
	png: "image/png",
	jpg: "image/jpeg",
	gif: "image/gif",
	svg: "image/svg+xml",
	ico: "image/x-icon",
};
