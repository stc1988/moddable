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
import config from "mc/config";

const smallest = 10;
const largest = Math.max(screen.height, screen.width);

const backgroundSkin = new Skin({fill: "white"});
const hintStyle = new Style({font: "16px OpenSans", color: "#808080", horizontal: "center"});

const styles = [];
function styleForSize(size) {
	let style = styles[size];
	if (!style)
		style = styles[size] = new Style({font: `${size}px OpenSans`, color: "black"});
	return style;
}

function distanceBetween(a, b) {
	const dx = a.x - b.x, dy = a.y - b.y;
	return Math.sqrt((dx * dx) + (dy * dy));
}

class TextBehavior extends Behavior {
	onCreate(label, $) {
		this.size = $.size;
		this.pinchFrom = 0;
	}
	// remember where a point falls within the word, as a fraction of it
	anchor(label, x, y) {
		this.anchorX = label.width ? (x - label.x) / label.width : 0.5;
		this.anchorY = label.height ? (y - label.y) / label.height : 0.5;
	}
	resize(label, size, x, y) {
		size = Math.round(size);
		if (size < smallest) size = smallest;
		else if (size > largest) size = largest;

		const style = styleForSize(size);
		const measured = style.measure(label.string);

		if (size !== this.size) {
			this.size = size;
			label.style = style;
		}

		label.moveBy(Math.round(x - (this.anchorX * measured.width) - label.x),
					Math.round(y - (this.anchorY * measured.height) - label.y));
	}
}

class GestureBehavior extends Behavior {
	onCreate(container) {
		this.touches = new Map();			// id -> {x, y, label}
	}
	labelAt(container, x, y) {
		for (let label = container.last; label; label = label.previous) {
			if ((x >= label.x) && (x < (label.x + label.width)) &&
				(y >= label.y) && (y < (label.y + label.height)))
				return label;
		}
	}
	touchesOn(label) {
		return [...this.touches.values()].filter(touch => label === touch.label);
	}
	claim(container) {
		const touches = [...this.touches.values()];
		if (2 !== touches.length)
			return;

		const [a, b] = touches;
		if (a.label && b.label)
			return;

		if (a.label || b.label)
			a.label = b.label = a.label ?? b.label;
		else
			a.label = b.label = this.labelAt(container, (a.x + b.x) / 2, (a.y + b.y) / 2);
	}
	restart() {
		for (const label of new Set([...this.touches.values()].map(touch => touch.label))) {
			if (!label)
				continue;

			const points = this.touchesOn(label);
			if (2 !== points.length) {
				label.behavior.pinchFrom = 0;
				continue;
			}

			label.behavior.pinchFrom = distanceBetween(points[0], points[1]);
			label.behavior.pinchSize = label.behavior.size;
			label.behavior.anchor(label, (points[0].x + points[1].x) / 2, (points[0].y + points[1].y) / 2);
		}
	}
	onTouchBegan(container, id, x, y, ticks) {
		this.touches.set(id, {x, y, label: this.labelAt(container, x, y)});
		this.claim(container);
		this.restart();
	}
	onTouchEnded(container, id, x, y) {
		this.touches.delete(id);
		this.restart();						// a remaining finger becomes a drag
	}
	onTouchMoved(container, id, x, y) {
		const touch = this.touches.get(id);
		if (!touch)
			return;

		const dx = x - touch.x, dy = y - touch.y;
		touch.x = x;
		touch.y = y;

		const label = touch.label;
		if (!label)							// this gesture began on nothing
			return;

		const points = this.touchesOn(label);
		if (1 === points.length) {			// one finger moves the word
			label.moveBy(dx, dy);
			return;
		}

		const now = distanceBetween(points[0], points[1]);
		const behavior = label.behavior;
		if (!behavior.pinchFrom || !now)
			return;

		behavior.resize(label, (behavior.pinchSize * now) / behavior.pinchFrom,
					(points[0].x + points[1].x) / 2, (points[0].y + points[1].y) / 2);
	}
}

const Text = Label.template($ => ({
	left: $.left, top: $.top, style: styleForSize($.size), string: $.string,
	Behavior: TextBehavior
}));

const PinchApplication = Application.template($ => ({
	skin: backgroundSkin, active: true,
	contents: [
		Container($, {
			left: 0, right: 0, top: 0, bottom: 0, active: true, multipleTouch: true,
			Behavior: GestureBehavior,
			contents: [
				Text({left: 20, top: 60, size: 40, string: "Embedded"}),
				Text({left: 130, top: 120, size: 40, string: "JavaScript"}),
			]
		}),
		Label($, {left: 0, right: 0, bottom: 4, height: 20, style: hintStyle, string: "© 2026 Moddable Tech, Inc."}),
	]
}));

export default new PinchApplication({}, {commandListLength: 4096, displayListLength: 8192, touchCount: config.touchCount});
