---
name: When to Optimize
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

As a rule, don't spend much time optimizing when you're starting a project.  Often optimized code is more complex and therefore more difficult to debug. You don't want that getting in your way while you're still getting your project to work.

Do take some time to learn about common optimization techniques. If you can easily apply them when writing your code, do that. Some of the techniques will become second nature.

Reserve your optimization effort for those parts of your code where it really matters. If a few lines of code run once at initialization, maybe they don't need to be as efficient as possible (unless, of course, your project takes too long to start up). Code that runs inside a loop tends to be more important to optimize.

Optimization is usually about getting performance to the point where it is "good enough." The definition of "good enough" depends on your project's goals. Code that is sufficiently optimized for one project may need to be tuned for another.

Optimization isn't only about performance. Many other aspects of an embedded system may need to be optimized, including memory, system resources, energy, and code size. These are not independent. For example, there is often a tradeoff between performance and memory use.