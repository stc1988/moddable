---
name: DER and PEM Certificates
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

TLS requires certificates to be in DER (binary) format. If you have a certificate in PEM (Base64 encoded) format, you need to convert it to DER before it can be used.

If possible, convert the PEM file to DER format before adding it to your project. There are many tools that can perform the conversion. [`openssl`](https://www.openssl.org) is a reliable choice. The following command line works for many certificates (substitute your PEM file path for `data.pem` and the desired output file path for `data.der`):

```shell
openssl x509 -inform pem -in data.pem -out data.der -outform der
```

---

It is sometimes necessary to convert the PEM to DER at runtime, for example, provisioning might deliver a certificate in PEM format. The Moddable SDK provides the [`pemToDER()`](../../crypt/crypt.md#transform-pemToDER) and [`privateKeyToPrivateKeyInfo()`](../../crypt/crypt.md#transform-privateKeyToPrivateKeyInfo) functions for these situations. These functions are part of the Crypt `Transform` class.
