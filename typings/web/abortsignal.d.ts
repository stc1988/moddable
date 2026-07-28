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

declare module "web/abortsignal" {
  class AbortSignal {
    readonly aborted: boolean;
    readonly reason: any;
    throwIfAborted(): void;
    static abort(reason?: any): AbortSignal;
    static timeout(milliseconds: number): AbortSignal;
    static any(signals: AbortSignal[]): AbortSignal;
  }

  class AbortController {
    readonly signal: AbortSignal;
    abort(reason?: any): void;
  }

  export { AbortSignal, AbortController };
}
