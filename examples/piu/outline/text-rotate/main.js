/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 *
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import {} from "piu/MC";
import {} from "piu/shape";
import {Outline} from "commodetto/outline";
import Resource from "Resource";

const font = new Resource("OpenSans-Regular.ttf");
font.size = 28;

const path = Outline.TextPath(font, "Moddable");
const fill = Outline.fill(path);
const stroke = Outline.stroke(path, 2, Outline.LINECAP_ROUND, Outline.LINEJOIN_ROUND);

const box = fill.bounds;
const middleX = box.x + Math.idiv(box.width, 2);
const middleY = box.y + Math.idiv(box.height, 2);

const secondsPerTurn = 5 / 2;
const secondsPerPulse = 6 / 2;

class SpinBehavior extends Behavior {
	onDisplaying(shape) {
		shape.start();
	}
	onTimeChanged(shape) {
		const seconds = shape.time / 1000;
		const angle = (seconds / secondsPerTurn) * 2 * Math.PI;
		const scale = 1.1 + (Math.sin((seconds / secondsPerPulse) * 2 * Math.PI) * 0.5);
		const x = shape.width / 2, y = shape.height / 2;

		shape.fillOutline = fill.clone(shape.fillOutline).transform(middleX, middleY, angle, scale, scale, x, y);
		shape.strokeOutline = stroke.clone(shape.strokeOutline).transform(middleX, middleY, angle, scale, scale, x, y);
	}
}

const TextApplication = Application.template($ => ({
	skin: new Skin({fill: "white"}),
	contents: [
		Shape($, {
			left: 0, right: 0, top: 0, bottom: 0, Behavior: SpinBehavior,
			skin: new Skin({fill: "#0000FF", stroke: "#0000FF"})
		}),
	]
}));

export default new TextApplication(null, {displayListLength: 8192, touchCount: 0, pixels: 240 * 64});
