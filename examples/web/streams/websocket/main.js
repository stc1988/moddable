import WebSocketStream from "web/websocketstream";
import Timer from "timer";

function delay(ms) {
	return new Promise((resolve, reject) => {
		Timer.set(() => resolve(), ms);
	});
}

const wss = new WebSocketStream("ws://websockets.chilkat.io/wsChilkatEcho.ashx");

const { readable, writable, extensions, protocol } = await wss.opened;
trace("opened\n");
const reader = readable.getReader();
const writer = writable.getWriter();

trace(`writer.write 5\n`);
await writer.write("Hello");
const data = new Uint8Array(1024);
for (let i = 0; i < 4; i++) {
	trace(`writer.write ${ data .byteLength }\n`);
	await writer.write(data);
}
trace(`writer.write 7\n`);
await writer.write("Goodbye");

while (true) {
	await delay(100);
	const { value, done } = await reader.read();
	if (done)
		break;
	if (value instanceof Uint8Array)
		trace(`reader.read binary ${value.byteLength}\n`);
	else {
		trace(`reader.read ${value}\n`);
		if (value.toUpperCase() == "GOODBYE")
			wss.close({ closeCode:1000, reason: "done" });
	}
}	
const { closeCode, reason } = await wss.closed;
trace(`closed ${closeCode} ${reason}\n`);
