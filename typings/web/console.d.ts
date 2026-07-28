/*
 * Copyright (c) 2026 Moddable Tech, Inc
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

export {};

declare module "web/console" {
}

declare global {
	interface Console {
		assert(condition?: boolean, ...data: any[]): void;
		clear(): void;
		count(label?: string): void;
		countReset(label?: string): void;
		debug(...data: any[]): void;
		dir(item?: any): void;
		dirxml(...data: any[]): void;
		error(...data: any[]): void;
		group(...data: any[]): void;
		groupCollapsed(...data: any[]): void;
		groupEnd(): void;
		info(...data: any[]): void;
		log(...data: any[]): void;
		table(tabularData?: any): void;
		time(label?: string): void;
		timeEnd(label?: string): void;
		timeLog(label?: string, ...data: any[]): void;
		trace(...data: any[]): void;
		warn(...data: any[]): void;
	}

	const console: Console;
}
