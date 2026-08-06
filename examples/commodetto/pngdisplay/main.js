/*
 * Copyright (c) 2016-2026 Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 * 
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <https://creativecommons.org/licenses/by/4.0>
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import HTTPServer from "embedded:network/http/server";
import Listener from "embedded:io/socket/listener";
import WiFi from "embedded:network/interface/wifi";
import WiFiAccessPoint from "embedded:network/interface/wifi/accesspoint";
import PNG from "commodetto/ReadPNG";
import Poco from "commodetto/Poco";
import Bitmap from "commodetto/Bitmap";
import Convert from "commodetto/Convert";
import config from "mc/config";

/*
	curl -T $MODDABLE/examples/commodetto/pngdisplay/test.png http://pngdisplay.local/upload

	Details about this application: https://www.moddable.com/blog/pngdisplay
*/
const HOSTNAME = "pngdisplay";
let poco;

function render(data) {
	const gray = poco.makeColor(128, 128, 128);
	const png = new PNG(data);
	const width = png.width, height = Math.min(png.height, poco.height), channels = png.channels;

	if ((8 !== png.depth) || ((3 !== channels) && (4 !== channels)) || png.palette)
		return fill(poco.makeColor(255, 0, 0));

	const pixelFormat = Bitmap[config.format];
	const convert = new Convert((3 == channels) ? Bitmap.RGB24 : Bitmap.RGBA32, pixelFormat);
	const scanOut = new ArrayBuffer((width * Bitmap.depth(pixelFormat)) >> 3);
	let bits;
	if ((0 === config.rotation) || (180 == config.rotation))
		bits = new Bitmap(width, 1, pixelFormat, scanOut, 0);
	else
		bits = new Bitmap(1, width, pixelFormat, scanOut, 0);
	const reverse = (config.rotation >= 180) ? new Uint16Array(scanOut) : undefined;

	fill(gray);
	for (let y = 0; y < height; y++) {
		convert.process(png.read(), scanOut);

		reverse?.reverse();

		poco.begin(0, y, width, 1);
		poco.drawBitmap(bits, 0, y);
		poco.end();
	}
}

function fill(color) {
	poco.begin();
	poco.fillRectangle(color, 0, 0, poco.width, poco.height);;
	poco.end();
}

export default function() {
	poco = new Poco(screen, {rotation: config.rotation});

	const wifi = new WiFi({});
	const hasStation = wifi.SSID;
	wifi.close();
	if (!hasStation) {
		new WiFiAccessPoint({
			SSID: HOSTNAME,
			channel: 8,
		});
	}

	const dnssd = new (device.network.dnssd.io)(device.network.dnssd);
	dnssd.claim({
		host: HOSTNAME,
		onReady() {
			fill(poco.makeColor(255, 255, 255));
			trace(`mDNS - claimed hostname is "${HOSTNAME}.local"\n`);
			dnssd.advertise({
				serviceType: "_http._tcp",
				name: HOSTNAME,
				host: HOSTNAME,
				port: 80
			});
		},
		onError() {
			trace("mDNS - failed to claim, give up\n");
			fill(poco.makeColor(255, 0, 0));
		}
	});

	new HTTPServer({
		io: Listener,
		port: 80,
		onConnect(connection) {
			connection.accept({
				onRequest(request) {
					this.isPut = "put" === request.method.toLowerCase();
					fill(this.isPut ? poco.makeColor(0, 255, 0) : poco.makeColor(255, 0, 0));

					const contentLength = request.headers.get("content-length");
					if (contentLength) {
						this.byteLength = parseInt(contentLength);
						try {
							this.png = new Uint8Array(new SharedArrayBuffer(this.byteLength));
						}
						catch {
							fill(poco.makeColor(255, 0, 0));
							return;
						}
						this.png.position = 0;
					}
				},
				onReadable(count) {
					if (!this.png) return;
					const data = new Uint8Array(this.read(count));
					this.png.set(data, this.png.position);
					this.png.position += data.byteLength;

					poco.begin(0, (poco.height - 4) >> 1, poco.width * (this.png.position / this.png.byteLength), 4);
					poco.fillRectangle(poco.makeColor(255, 255, 255), 0, 0, poco.width, poco.height);
					poco.end();
				},
				onResponse(response) {
					try {
						if (this.png)
							render(this.png.buffer);
						else
							response.status = 500;
					}
					catch {
						response.status = 500;
					}
					finally {
						delete this.png;
						response.headers.set("content-length", "0");
						this.respond(response);
					}
				}
			});
		}
	});

	fill(poco.makeColor(64, 64, 64));
};
