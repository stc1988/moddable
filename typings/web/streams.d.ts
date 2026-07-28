/*
* Copyright (c) 2026 Moddable Tech, Inc.
*
*   This file is part of the Moddable SDK Tools.
*
*   The Moddable SDK Tools is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   The Moddable SDK Tools is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with the Moddable SDK Tools.  If not, see <http://www.gnu.org/licenses/>.
*
*/

declare module "web/streams" {
  import type { AbortSignal } from "web/abortsignal";

  interface QueuingStrategyInit {
    highWaterMark: number;
  }

  interface QueuingStrategy<T = any> {
    highWaterMark?: number;
    size?(chunk: T): number;
  }

  interface UnderlyingSource<R = any> {
    start?(controller: ReadableStreamDefaultController<R>): void | Promise<void>;
    pull?(controller: ReadableStreamDefaultController<R>): void | Promise<void>;
    cancel?(reason?: any): void | Promise<void>;
    type?: undefined;
  }

  interface UnderlyingByteSource {
    start?(controller: ReadableByteStreamController): void | Promise<void>;
    pull?(controller: ReadableByteStreamController): void | Promise<void>;
    cancel?(reason?: any): void | Promise<void>;
    type: "bytes";
    autoAllocateChunkSize?: number;
  }

  interface UnderlyingSink<W = any> {
    start?(controller: WritableStreamDefaultController): void | Promise<void>;
    write?(chunk: W, controller: WritableStreamDefaultController): void | Promise<void>;
    close?(): void | Promise<void>;
    abort?(reason?: any): void | Promise<void>;
  }

  interface Transformer<I = any, O = any> {
    readableType?: undefined;
    writableType?: undefined;
    start?(controller: TransformStreamDefaultController<O>): void | Promise<void>;
    transform?(chunk: I, controller: TransformStreamDefaultController<O>): void | Promise<void>;
    flush?(controller: TransformStreamDefaultController<O>): void | Promise<void>;
    cancel?(reason?: any): void | Promise<void>;
  }

  interface StreamPipeOptions {
    preventClose?: boolean;
    preventAbort?: boolean;
    preventCancel?: boolean;
    signal?: AbortSignal;
  }

  interface ReadableWritablePair<R = any, W = any> {
    readable: ReadableStream<R>;
    writable: WritableStream<W>;
  }

  type ReadableStreamReadValueResult<T> = { done: false; value: T };
  type ReadableStreamReadDoneResult<T> = { done: true; value: T | undefined };
  type ReadableStreamReadResult<T> =
    | ReadableStreamReadValueResult<T>
    | ReadableStreamReadDoneResult<T>;

  class ReadableStream<R = any> {
    constructor(underlyingSource?: UnderlyingSource<R>, strategy?: QueuingStrategy<R>);
    constructor(underlyingSource: UnderlyingByteSource, strategy?: QueuingStrategy<Uint8Array>);
    readonly locked: boolean;
    cancel(reason?: any): Promise<void>;
    getReader(): ReadableStreamDefaultReader<R>;
    getReader(options: { mode?: undefined }): ReadableStreamDefaultReader<R>;
    getReader(options: { mode: "byob" }): ReadableStreamBYOBReader;
    pipeThrough<T>(transform: ReadableWritablePair<T, R>, options?: StreamPipeOptions): ReadableStream<T>;
    pipeTo(destination: WritableStream<R>, options?: StreamPipeOptions): Promise<void>;
    tee(): [ReadableStream<R>, ReadableStream<R>];
    values(options?: { preventCancel?: boolean }): AsyncIterableIterator<R>;
    [Symbol.asyncIterator](): AsyncIterableIterator<R>;
    static from<T>(iterable: Iterable<T> | AsyncIterable<T>): ReadableStream<T>;
  }

  class ReadableStreamDefaultReader<R = any> {
    constructor(stream: ReadableStream<R>);
    readonly closed: Promise<undefined>;
    cancel(reason?: any): Promise<void>;
    read(): Promise<ReadableStreamReadResult<R>>;
    releaseLock(): void;
  }

  class ReadableStreamBYOBReader {
    constructor(stream: ReadableStream<Uint8Array>);
    readonly closed: Promise<undefined>;
    cancel(reason?: any): Promise<void>;
    read<T extends ArrayBufferView>(view: T): Promise<ReadableStreamReadResult<T>>;
    releaseLock(): void;
  }

  class ReadableStreamDefaultController<R = any> {
    readonly desiredSize: number | null;
    close(): void;
    enqueue(chunk?: R): void;
    error(e?: any): void;
  }

  class ReadableByteStreamController {
    readonly byobRequest: ReadableStreamBYOBRequest | null;
    readonly desiredSize: number | null;
    close(): void;
    enqueue(chunk: ArrayBufferView): void;
    error(e?: any): void;
  }

  class ReadableStreamBYOBRequest {
    readonly view: ArrayBufferView | null;
    respond(bytesWritten: number): void;
    respondWithNewView(view: ArrayBufferView): void;
  }

  class WritableStream<W = any> {
    constructor(underlyingSink?: UnderlyingSink<W>, strategy?: QueuingStrategy<W>);
    readonly locked: boolean;
    abort(reason?: any): Promise<void>;
    close(): Promise<void>;
    getWriter(): WritableStreamDefaultWriter<W>;
  }

  class WritableStreamDefaultWriter<W = any> {
    constructor(stream: WritableStream<W>);
    readonly closed: Promise<undefined>;
    readonly desiredSize: number | null;
    readonly ready: Promise<undefined>;
    abort(reason?: any): Promise<void>;
    close(): Promise<void>;
    releaseLock(): void;
    write(chunk?: W): Promise<void>;
  }

  class WritableStreamDefaultController {
    readonly signal: AbortSignal;
    error(e?: any): void;
  }

  class TransformStream<I = any, O = any> {
    constructor(
      transformer?: Transformer<I, O>,
      writableStrategy?: QueuingStrategy<I>,
      readableStrategy?: QueuingStrategy<O>
    );
    readonly readable: ReadableStream<O>;
    readonly writable: WritableStream<I>;
  }

  class TransformStreamDefaultController<O = any> {
    readonly desiredSize: number | null;
    enqueue(chunk?: O): void;
    error(reason?: any): void;
    terminate(): void;
  }

  class ByteLengthQueuingStrategy {
    constructor(init: QueuingStrategyInit);
    readonly highWaterMark: number;
    size(chunk: ArrayBufferView): number;
  }

  class CountQueuingStrategy {
    constructor(init: QueuingStrategyInit);
    readonly highWaterMark: number;
    size(): number;
  }

  export {
    ReadableStream,
    ReadableStreamDefaultReader,
    ReadableStreamBYOBReader,
    ReadableStreamDefaultController,
    ReadableByteStreamController,
    ReadableStreamBYOBRequest,
    WritableStream,
    WritableStreamDefaultWriter,
    WritableStreamDefaultController,
    TransformStream,
    TransformStreamDefaultController,
    ByteLengthQueuingStrategy,
    CountQueuingStrategy,
  };
  export type {
    QueuingStrategyInit,
    QueuingStrategy,
    UnderlyingSource,
    UnderlyingByteSource,
    UnderlyingSink,
    Transformer,
    StreamPipeOptions,
    ReadableWritablePair,
    ReadableStreamReadResult,
    ReadableStreamReadValueResult,
    ReadableStreamReadDoneResult,
  };
}
