---
name: Send Request Body using fetch()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Set the request body to text by setting the `body` option to a string.

```js
import { fetch, Headers } from "fetch";

const response = await fetch("https://httpbin.org/post", {
	method: "POST",
	headers: new Headers([["Content-Type", "text/plain"]]),
	body: "request body"
});
trace(await response.text(), "\n");
```

---

Set the request body to binary data by setting the `body` option to an `ArrayBuffer`.

```js
import { fetch, Headers } from "fetch";

const response = await fetch("https://httpbin.org/post", {
	method: "POST",
	headers: new Headers([["Content-Type",
					"application/octet-stream"]]),
	body: Uint8Array.of(0, 1, 2, 3).buffer
});
trace(await response.text(), "\n");
```

---

Set the request body to form-encoded URL search parameters by setting the `body` option to an instance of the standard [`URLSearchParams`](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams) class.

```js
import { fetch, Headers } from "fetch";
import { URLSearchParams } from "url";

const response = await fetch("https://httpbin.org/post", {
	method: "POST",
	headers: new Headers([["Content-Type",
		"application/x-www-form-urlencoded;charset=UTF-8"]]),
	body: new URLSearchParams([
		["Date", Date()],
		["Input", "Look! Input!"]
	])
});
trace(JSON.stringify(await response.json()), "\n");
```

---

Use the stream-enabled version of `fetch()` to stream the request body.

This example creates a `ReadableStream` that generates ten short strings for the request body.

This pattern can be used to stream a file of binary data as the request body by enqueuing `ArrayBuffer` instances instead of strings.

```js
import { fetch } from "web/fetch/streams";
import { ReadableStream } from "web/streams";

let count = 0;
const response = await fetch("http://httpbin.org/put", {
	method:"PUT",
	body: new ReadableStream({
		pull(controller) {
			if (count < 10) {
				controller.enqueue(
					`Request body part #${count}.\n`);
				count++;
			}
			else
				controller.close();
		}
	})
});
const text = await response.text();
trace(text, "\n");
```
