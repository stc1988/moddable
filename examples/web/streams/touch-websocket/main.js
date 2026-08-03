/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 *
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import config from "mc/config";
import Timer from "timer";
import { ReadableStream, TransformStream } from "web/streams";
import WebSocketStream from "web/websocketstream";

async function test() {
	const touchStream = new ReadableStream({
		start(controller) {
			function onSample() {
				const points = touch.sample();
				if (points)
					controller.enqueue(points);
			}
			const touch = new device.sensor.Touch({ onSample });
			if (!touch.configuration?.interrupt)
				touch.timer = Timer.repeat(onSample, 16);
		}
	});
	
	const transformStream = new TransformStream({
		transform(points, controller) {
			controller.enqueue(JSON.stringify(points));
		},
	});
	const jsonStream = touchStream.pipeThrough(transformStream);
	
	const wss = new WebSocketStream(`ws://${config.server}:8081`);
	const { writable } = await wss.opened;
	jsonStream.pipeTo(writable);	
}

if (config.server)
	test();
else
	trace("Add server=<ip address> to the mcconfig command line!");

