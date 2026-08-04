---
name: Optimizing Embedded JavaScript
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

These guides explain techniques to improve performance of Embedded JavaScript running on microcontrollers with the Moddable SDK. Unlike the other guides, they are not about a particular API or module — they are patterns you apply to your code. Each shows a "before" and "after" so you can see the change, Most apply to any JavaScript engine while being especially valuable on resource-constrained devices.

Optimization is about more than speed. Memory, system resources, energy, and code size all matter on an embedded device. They often trade off against one another. Start by understanding when and where optimization is worthwhile, then explore specific techniques as your measurements justify them.

## Before You Optimize

- [When to Optimize](./whentooptimize.md)
- [Know Where to Optimize](./wheretooptimize.md)

## Techniques

- [Loop through an Array](./looping.md)
- [Iterate Over a String](./stringiterate.md)
- [Build a String](./stringconcat.md)
- [Avoid Copying Buffers](./nocopybuffers.md)
- [Accessing Properties](./accessproperties.md)
- [Map versus Object](./maps.md)
- [Append to an Array](./arraypush.md)
- [Operate on Bits](./bitoperations.md)
- [Define Class Methods](./definingclasses.md)

## Learn More

- Documentation
	- [XS Profiler](../../xs/XS%20Profiler.md) in Moddable SDK
	- [xsbug debugger](../../xs/xsbug.md) in Moddable SDK
- Articles
	- [Deliver High-Performance Products with the XS Profiler](https://www.moddable.com/blog/profiler/)
	- [Optimizing Life Using the XS Performance Profiler](https://www.moddable.com/blog/optimizing-life/)
