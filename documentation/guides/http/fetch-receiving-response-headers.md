---
name: Receive Response Headers using fetch()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

HTTP response headers are available on the `response` returned by `fetch()`. The `headers` property is an instance of the standard [Headers](https://developer.mozilla.org/en-US/docs/Web/API/Headers) class.

This example checks the `content-type` header. If it is JSON, the response is parsed as JSON; if not, it is retrieved as text.

```js
import { fetch } from "fetch";

const response = await fetch("http://httpbin.org/json");
if (response.headers.get("Content-Type") ===
			"application/json")
	trace(JSON.stringify(await response.json()), "\n");
else
	trace(await response.text(), "\n");
```

---

To see all headers, use the `forEach()` method of the `Headers` class.

```js
import { fetch } from "fetch";

const response = await fetch("http://httpbin.org/json");
response.headers.forEach(
	(value, key) => trace(`${key}: ${value}\n`));
```
