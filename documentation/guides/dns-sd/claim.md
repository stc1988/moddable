---
name: Claim Local Name
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

These examples use a shared `dnssd` instance, as shown in [Creating a DNS-SD Instance](./index.md#creating-a-dns-sd-instance).

Use `claim()` to begin the process of claiming a local name. Local names have a `.local` suffix, such as `example.local`, and are only accessible on the local network. When you call `claim()` omit the `.local` suffix from the `host`.

The claim process fails if another device on the local network has already claimed the name.

```js
dnssd.claim({
	host: "example",
	onReady() {
		trace(`"example.local" claimed\n`);
	},
	onError() {
		trace(`couldn't claim "example.local"\n`);
	}
});
```

---

To release a claim to a name, call `close()` on the returned claim instance. One reason to release a claim is if your project allows the user to change the local name of their device.

```js
const exampleClaim = dnssd.claim({
	host: "example"
});

// some time later
exampleClaim.close();
```

---

If a claim fails, retry with a different name. A common approach is appending a number to the name when retrying.

```js
function doClaim(name, attempt = 0) {
	const host = attempt ? `${name}-${attempt}` : name;
	dnssd.claim({
		host,
		onReady() {
			trace(`"${host}.local" claimed\n`);
		},
		onError() {
			trace(`couldn't claim "${host}.local"\n`);
			doClaim(name, ++attempt);
		}
	});
}
doClaim("example");
```
