---
name: Files
SPDX-FileCopyrightText: Copyright (c) 2026 Moddable Tech, Inc.
updated: 2026-08-04
---

The ECMA-419 embedded standard [Files](https://419.ecma-international.org/#storage-files) API works with both files and directories. The API is modeled on POSIX file semantics, so it is familiar to most developers.

File and directory operations are performed on a directory instance. The global `device.files` is the root directory instance.

## Files

- [Create, Open, and Close File](./file-create-open-close.md)
- [Read File](./file-read.md)
- [Write File](./file-write.md)
- [Delete File](./file-delete.md)
- [Get File Information](./file-info.md)

## Directories

- [Create Directory](./dir-create.md)
- [Enumerate Directory](./dir-enumerate.md)
- [Delete Directory](./dir-delete.md)
- [Open Directory](./dir-open.md)

## Paths

Paths are [subpath strings](https://419.ecma-international.org/#storage-file-subpath) that follow a few rules:

- All paths are relative to the directory instance they are used with. Absolute paths, `"./"`, and `"../"` are not allowed.
- The path separator is always `"/"`, regardless of the host file system separator.
- Case sensitivity of names depends on the underlying host file system. For portability, be consistent in your use of case to avoid surprises.

## Building

Include the Files manifest in your project's `manifest.json`. Including the manifest initializes the host provider `device.files`:

	$(MODDABLE)/modules/io/files/manifest.json

The manifest selects the implementation for your target: POSIX on macOS and Linux, LittleFS on ESP32, and platform file systems on Pebble OS and Zephyr.

## Learn More

- Standard
	- [Files](https://419.ecma-international.org/#storage-files) in ECMA-419
- Implementation
	- [Files](../../../modules/io/files/) in Moddable SDK
- TypeScript Declaration
	- [Files](../../../typings/embedded/storage/files.d.ts) in Moddable SDK
