---
name: Logging with trace()
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

`trace()` is a global function built into the XS JavaScript engine. Like `printf()` in C, `trace()` doesn't append a linefeed to the output.

```js
trace("hello, world.\n");
-> hello, world.

trace("hello,");
trace(" world.");
trace("\n");
-> hello, world.
```

---

`trace()` accepts an unlimited number of arguments, and concatenates them together without a separator. It is more efficient to pass several separate arguments to `trace()`, rather than adding them together and passing a single string.

```js
const world = "world.";
trace("hello, ", world, "\n");
-> hello, world.
```

---

`trace()` works with template literals.

```js
const world = "world";
trace(`hello ${world}.\n`);
```

---

If an argument to `trace()` is not a string, it is converted to a string. However, `trace()` cannot coerce a `Symbol` to a string.

```js
trace("Time now: ", new Date, "\n");
```

---

When tracing to xsbug, the log pane colorizes lines that begin with `<error>`, `<warn>`, and `<info>`.

```js
trace(`<error>Error!\n`);
trace(`<warn>Warning!\n`);
trace(`<info>Info!\n`);
```

---

The Messages pane in xsbug shows log messages like a conversation (watch [the video](https://www.youtube.com/watch?v=2tSRVYtm-ZU)), with bubbles on the left and right of the pane. This visualization is invaluable for debugging two-way conversations, for example, between the main thread and a worker, or the send and receive sides of a network connection. Use `trace.right()`, `trace.left()`, and `trace.center()`.

Linefeeds are unnecessary except to put line breaks inside a message bubble.

```js
trace.center(`Conversation start`);
trace.left("sent - msg 1");
trace.right("received - msg 2");
```
