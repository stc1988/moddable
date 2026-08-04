---
name: Know Where to Optimize
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

It's usually easy to know that code isn't running fast enough. You can feel it. Determining why the code is running slowly can be difficult. You may have an intuition about what's making things slow. Wouldn't it be nice to know for sure, before taking time to optimize the code?

Fortunately, there's a tool to help: the performance profiler built into the xsbug JavaScript debugger. It measures the performance of JavaScript running on embedded devices and in the desktop simulator. You don't need any special set-up to run it – just click start and watch the results roll in.

We've got a couple of articles to get you started with the profiler.

- [Deliver High-Performance Products with the XS Profiler](https://www.moddable.com/blog/profiler/) introduces the profiler. It includes a demo video that shows how easy it is to zoom in on the hotspots in your code.
- [Optimizing Life Using the XS Performance Profiler](https://www.moddable.com/blog/optimizing-life/) is a step-by-step example that uses the profiler to achieve a **10x** performance increase.

Sometimes you may not see an immediate solution to the performance hotspot because most of the time is being taken by a system library, rather than your code. There's no single solution, but there are some good places to start looking.

- If the garbage collector is taking a significant amount of time, you have a couple of options. First, you may be able to modify your code to allocate less memory. See [Avoid Copying Buffers](./nocopybuffers.md) and [Building a String](./stringconcat.md), for example.
- If a particular function is slow, sometimes an easy option is to call it less frequently. Alternatively, check the documentation as the function may have options that can lower its overhead, such as [the `push()` method of arrays.](./arraypush.md)
