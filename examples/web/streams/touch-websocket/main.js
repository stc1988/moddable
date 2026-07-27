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

