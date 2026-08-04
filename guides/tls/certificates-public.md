---
name: Include Public Certificates
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

By default, projects do not include any TLS certificates. That means that to connect securely you must first add the necessary certificates to your project.

Public certificates are stored as Resources that are globally available within the project without being registered. See [Include Private Certificates](./certificates-private.md) to associate a certificate with a connection.

---

The easiest way to determine which certificate you need is to try to establish a secure connection to the server you want to use.

If the required certificate cannot be found for the domain, TLS throws an exception. For well-known public certificates, the exception includes the name of the required certificate: `Resource not found: ca109.der`.

Certificates with names like `ca[number].der` are already in the Moddable SDK. You just need to add the certificate to the `data` section of your project's manifest.

```json
"data": {
    "*": [
        "$(MODULES)/crypt/data/ca109.der"
    ]
}
```

---

Include several certificates by adding each to the manifest.

```json
"data": {
    "*": [
        "$(MODULES)/crypt/data/ca37.der",
        "$(MODULES)/crypt/data/ca109.der"
    ]
}
```
