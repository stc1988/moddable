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

declare module "web/websocketstream" {
  import type { AbortSignal } from "web/abortsignal";
  import type { ReadableStream, WritableStream } from "web/streams";

  interface WebSocketStreamOptions {
    protocols?: string | string[];
    signal?: AbortSignal;
    ws?: any;
    wss?: any;
  }

  interface WebSocketCloseInfo {
    closeCode?: number;
    reason?: string;
  }

  interface WebSocketConnection {
    readable: ReadableStream<string | Uint8Array>;
    writable: WritableStream<string | ArrayBuffer | ArrayBufferView>;
    extensions: string;
    protocol: string;
  }

  class WebSocketStream {
    constructor(url: string, options?: WebSocketStreamOptions);
    readonly url: string;
    readonly opened: Promise<WebSocketConnection>;
    readonly closed: Promise<Required<WebSocketCloseInfo>>;
    close(closeInfo?: WebSocketCloseInfo): void;
  }

  export default WebSocketStream;
}
