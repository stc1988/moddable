---
name: Send Request Headers using fetch()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

Use the standard [Headers](https://developer.mozilla.org/en-US/docs/Web/API/Headers) class to create a collection of HTTP headers, then pass that to `fetch()`.

```js
import { fetch, Headers } from "fetch";

const headers = new Headers([
	["Content-Type", "text/plain"],
	["Date", Date()],
	["User-Agent", "fetch example"]
]);

const response = await fetch("http://httpbin.org/encoding/utf8", {headers});
trace(await response.text(), "\n");
```
