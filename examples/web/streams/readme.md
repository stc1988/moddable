# Streams
Revised: July 15, 2026

This directory contains several Moddable SDK example applications that use [web streams](https://streams.spec.whatwg.org). The examples have all been successfully run on [Moddable Two](https://www.moddable.com/moddable-two) and [Moddable Six](https://www.moddable.com/moddable-six), development boardd built around the ESP32 microcontroller. They should run on other ESP32-based devices, though configuration changes may be necessary. Because of code size and RAM requirements, the examples may not fit into less capable microcontrollers.

<a id="modules"></a>
## `modules`
The `modules` directory contains two modules that mix ECMA-419 class patterns and web streams.

### iostreams.js

The IO Streams module supports ECMA-419 IO using streams. The module exports two mixins, which create `ReadableStream` and `WritableStream` subclasses based on a class that conforms to the [ECMA-419 IO class pattern](https://419.ecma-international.org/#-9-io-class-pattern). The `button` example shows how to use these mixins.

### sensorstreams.js

The IO Streams module supports ECMA-419 sensors using streams. The module exports a mixin which creates a `ReadableStream` subclass based on an a class that conforms to the [ECMA-419 Sensor class pattern](https://419.ecma-international.org/#-13-sensor-class-pattern). The `touch` example shows how to use this mixin.

<a id="button"></a>
## `button`

This example revisits Moddable [IO button example](https://github.com/Moddable-OpenSource/moddable/tree/public/examples/io/digital/button). It turns an LED on and off based on the state of a button.

Using to the mixins exported by `iostreams.js`, the `Digital` class becomes both a `ReadableStream` subclass for the button and a `WritableStream` subclass for the LED.

There is also a `TransformStream` to invert the value.

To build:

```shell
cd $MODDABLE/examples/web/streams/button
mcconfig -d -m -p esp32/moddable_two_io
```

The button example does not run on Moddable Six because there is no led pin.

Note that there is no console output when the LED changes state; you must watch the LED itself to see the state changes.

<a id="touch"></a>
## `touch`

Thanks to the mixin exported by `sensorstream.js`, the `Touch` class from `embedded:sensor/Touch/FT6x06` becomes a `ReadableStream` subclass.

Since `ReadableStream` provides an async iterator, the stream of points is read with a `for await` loop

To build:

```shell
cd $MODDABLE/examples/web/streams/touch
mcconfig -d -m -p esp32/moddable_six
```

<a id="fetch"></a>
## `fetch`

This example implements the web standard[ `fetch()` function](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API) to make an HTTP request, using Moddable SDK's implementation of the [ECMA-419 HTTP Client class pattern](https://419.ecma-international.org/#-20-http-client-class-pattern) and the `ReadableStream` class.

To build:

```shell
cd $MODDABLE/examples/web/streams/fetch
mcconfig -d -m -p esp32/moddable_six ssid=<SSID> password=<PASSWORD>
```

The fetch example also runs on the simulator:

```shell
mcconfig -d -m
```

<a id="decompress"></a>
## `decompress`

This example exercises the `DecompressionStream` class.

To build:

```shell
cd $MODDABLE/examples/web/streams/decompress
mcconfig -d -m -p esp32/moddable_six
```

The decompress example also runs on the simulator:

```shell
mcconfig -d -m
```

<a id="websocket"></a>
## `websocket`

This example exercises the `WebSocketStream` class.

The `WebSocketStream` class implements the 
[WebSocketStream proposal](https://developer.mozilla.org/en-US/docs/Web/API/WebSocketStream) with the [ECMA-419 WebSocket Client](https://419.ecma-international.org/#websocket-client).

To build:

```shell
cd $MODDABLE/examples/web/streams/websocket
mcconfig -d -m -p esp32/moddable_six ssid=<SSID> password=<PASSWORD>
```

The fetch example also runs on the simulator:

```shell
mcconfig -d -m
```

The write loop uses promises to wait for data to be sent:

```javascript
const data = new Uint8Array(1024);
for (let i = 0; i < 4; i++) {
	await writer.write(data);
}
```
Without `await`, the writable stream would buffer the data.

The read loop uses a delay to simulate a slow consumer and to take advantage of the back pressure mechanism.

```javascript
while (true) {
	await delay(100);
	const { value, done } = await reader.read();
	if (done)
		break;
}
```

<a id="websocket"></a>
## `touch-websocket`

This example combines sensor and WebSocket streams in order to show how the stream infrastructure can be used to propagate sensor data.

This example requires a simple WebSocket server to receive sensor data from the device and send sensor data to a browser.

Launch the server:

```shell
cd $MODDABLE/examples/web/streams/touch-websocket/server
npm install
node server.js
```
Open `localhost:8081/index.html` in your browser.

Then build and run the example:

```shell
cd $MODDABLE/examples/web/streams/touch-websocket
mcconfig -d -m -p esp32/moddable_six ssid=<SSID> password=<PASSWORD> server=<IP_ADDRESS>
```

where `<IP_ADDRESS>` is the IP address of your computer on your local network. You can get it for instance with:

```shell
ipconfig getifaddr en0
```

When you touch the screen of your device, you will see fingerprints in the browser.

![browser](touch-websocket/browser.jpg)

Firstly there is a readable stream that enqueues touch samples:

```javascript
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
```
To run on both Moddable Two and Six, the stream use a timer or an interrupt.

Then there is a `TransformStream` to convert samples into `JSON` with `pipeThrough`.
```javascript	
const transformStream = new TransformStream({
	transform(points, controller) {
		controller.enqueue(JSON.stringify(points));
	},
});
const jsonStream = touchStream.pipeThrough(transformStream);
```
That demonstrates the flexibility of the stream architecture. You could of course directly enqueue samples in `JSON`.

Eventually, the `JSON` stream pipes to the writable stream of a `WebSocketStream`
```javascript	
const wss = new WebSocketStream(`ws://${config.server}:8081`);
const { writable } = await wss.opened;
jsonStream.pipeTo(writable);	
```












