---
name: Implementing ECMA-419 Modules
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

ECMA-419, [ECMAScript® Embedded Systems API Specification](https://419.ecma-international.org) defines standard APIs for Embedded JavaScript. This guide describes best practices for implementing these APIs. The focus is conformance: ensuring the implementations match the standard. That's important because developers depend on ECMA-419's common patterns. Fortunately, the implementation details are easy to get right, once you understand what the standard requires.

- [Constructor Sets `target`](./target.md)
- [Constructor IO](./io.md)
- [Constructor Clean-up on Failure](./cleanup.md)
- [`close()` and `[Symbol.dispose]`](./close.md)
- [Calling Callbacks](./callbacks.md)
- [Setting Options with `configure()`](./configure.md)
- [Keep Instance Surface Clean](./private.md)

These guides focus on modules implemented in JavaScript. ECMA-419 modules may also be implemented in C using the Moddable SDK's [XS in C](../../xs/XS%20in%20C.md) API. Whatever language is used, the conformance requirements are the same.

When implementing shared code, there is a strong tendency to try to help the user, for example by tolerating incorrectly formed inputs or making a guess to resolve an ambiguity. This kind of help is problematic for portability, as it means that two different implementations of, for example, a temperature sensor might behave differently with the same inputs. As a rule, the ECMA-419 implementation favors throwing an exception in these cases, so that the caller fixes their code. ECMA-419 provides complete flexibility on the error thrown, so you can provide as helpful a message as you feel is appropriate.

If at all possible, your module should be compatible with the Moddable SDK's preload feature. This allows your implementation to reside fully in flash memory, saving limited RAM space and reducing start-up time. All of the recommendations in these guides are compatible with preload. Check out the [preload documentation](../../xs/preload.md) for details.
