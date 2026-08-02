/*
 * Copyright (c) 2021-2026  Moddable Tech, Inc.
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

import { fetch, Headers } from "fetch";
import { URLSearchParams } from "url";
import { AbortController } from "web/abortsignal";
import { ReadableStream } from "web/streams";
import TextDecoderStream from "web/textdecoderstream";

async function readData(url, info) {
	try {
		const response = await fetch(url, info);
		const transformedBody = response.body.pipeThrough(new TextDecoderStream());
	
		const reader = transformedBody.getReader();
		while (true) {
			const { done, value } = await reader.read();
			if (value) {
				trace(value);
			}
			if (done) {
				trace("\n");
				return;
			}
		}
	}
	catch(e) {
		trace(`${e}\n`);
	}
}
let controller = new AbortController();
readData("http://httpbin.org/encoding/utf8", { signal: controller.signal });

// Always-compressed endpoints to verify the decompression path.
async function fetchJson(label, url) {
	try {
		const r = await fetch(url);
		trace(`\n${label} content-encoding: ${r.headers.get("content-encoding")}\n`);
		const text = await r.text();
		trace(`${label} body: ${text}\n`);
	}
	catch (e) {
		trace(`${label} threw: ${e}\n`);
	}
}
fetchJson("gzip",    "http://httpbin.org/gzip");
fetchJson("deflate", "http://httpbin.org/deflate");

fetch("http://httpbin.org/put", { method:"PUT", body:"This is no data!" })
.then(response => {
	trace(`\n${response.url} ${response.status} ${response.statusText}\n\n`);
	response.headers.forEach((value, key) => trace(`${key}: ${value}\n`));
	trace("\n");
	return response.text();
})
.then(text => {
	trace(`${text}\n`);
})

let count = 0;
const stream = new ReadableStream({
	pull(controller) {
		if (count < 10) {
			controller.enqueue(`${count} This is no data!`);
			count++;
		}
		else
			controller.close();
	},
});

fetch("http://httpbin.org/put", { method:"PUT", body:stream })
.then(response => {
	trace(`\n${response.url} ${response.status} ${response.statusText}\n\n`);
	response.headers.forEach((value, key) => trace(`${key}: ${value}\n`));
	trace("\n");
	return response.text();
})
.then(text => {
	trace(`${text}\n`);
})


