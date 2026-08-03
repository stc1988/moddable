import {serial} from "web/serial";
import TextDecoder from "text/decoder";
import TextEncoder from "text/encoder";
import Timer from "timer";

trace("started\n");

try {
	const port = await serial.requestPort();
	await port.open({baudRate: 115200 * 1, bufferSize: 20});
	let total = 0;
	const reader = port.readable.getReader({ mode: "byob" });
	
	let decoder = new TextDecoder();
	let text = "";

	let buffer = new ArrayBuffer(128);
	const readLoop = async () => {
		try {
			while (true) {
				const { value, done } = await reader.read(new Uint8Array(buffer));
				if (done) {
					trace(`Read loop clean exit ${total}\n`);
					break;
				}
				text += decoder.decode(value, { stream: true });
				const lines = text.split("\n");
				text = lines.pop();
				lines.forEach(line => trace("Received:", line, "\n"));

				buffer = value.buffer;
				total += value.byteLength;
				trace("Read loop ", value.byteLength, "\n")
			}
		} catch (error) {
			trace(`Read loop error: ${error}\n`);
		} finally {
    		reader.releaseLock();
		}
	};
	readLoop();

	await new Promise(resolve => Timer.set(resolve, 2000));
	await reader.cancel();
	await port.close();
	trace("Clean exit\n")
} catch (err) {
	trace("Serial error:", err);
}
