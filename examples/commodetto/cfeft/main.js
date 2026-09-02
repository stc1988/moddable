/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

import Poco from "commodetto/Poco";
import Resource from "Resource";

const poco = new Poco(screen, {displayListLength: 8192});
const white = poco.makeColor(255, 255, 255);
const black = poco.makeColor(0, 0, 0);
const orange = poco.makeColor(255, 128, 0);

// the font is the TrueType file itself. the size travels with it.
const font = new Resource("OpenSans-Regular.ttf");

poco.begin();
	poco.fillRectangle(white, 0, 0, poco.width, poco.height);

	let y = 4;
	for (let size of [12, 16, 24, 32, 48]) {
		font.size = size;
		poco.drawText(`Size ${size}`, font, (32 === size) ? orange : black, 4, y);
		y += size + 4;
	}

	font.size = 20;
	const text = "The quick brown fox";
	poco.drawText(text, font, black, 4, y);
	trace(`width of "${text}" at 20px: ${poco.getTextWidth(text, font)}\n`);
poco.end();
