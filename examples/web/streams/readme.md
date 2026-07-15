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

