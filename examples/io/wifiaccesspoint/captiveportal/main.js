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

import WiFiAccessPoint from "embedded:network/interface/wifi/accesspoint";
import WiFi from "embedded:network/interface/wifi";

import UDP from "embedded:io/socket/udp";
import DNSServer from "embedded:network/dns/server/udp"

import Listener from "embedded:io/socket/listener";
import HTTPServer from "embedded:network/http/server"
import WebPage from "embedded:network/http/server/options/webpage";

import WebSocketHandshake from "embedded:network/http/server/options/websocket";
import WebSocket from "WebSocket";

const provisioningPage = ArrayBuffer.fromString(`
<html><body>
<header>Welcome to provisioning!</header>

<pre id="log"></pre>

<script>
function log(...args) {
	document.getElementById("log").textContent += args.join(" ") + String.fromCharCode(10);
}
const socket = new WebSocket("ws://" + location.host + "/ws");
log("WebSocket Created");
socket.addEventListener("open", () => {
	log("Connected");
	socket.send(JSON.stringify({
		event: "scan"
	}));
});
socket.addEventListener("message", event => {
	const msg = JSON.parse(event.data);
	log("Received:", JSON.stringify(msg, undefined, "  "));
	if ("scan" === msg.event) {
		socket.send(JSON.stringify({
			event: "connect",
			SSID: "my SSID",
			password: "my PASSWORD"
		}));
	}
	else if (("wifi-change" === msg.event) && ("connection" === msg.property) && (parseInt(msg.value) >= 500)) {
		log("Provisioning complete!");
		socket.send(JSON.stringify({
			event: "terminate"
		}));
	}
});
socket.addEventListener("close", event => {
	log("Disconnected:", event.code);
});
socket.addEventListener("error", event => {
    log("Error:", event.type);
});
</script>
</body></html>
`);

const accessPoint = new WiFiAccessPoint({
	SSID: "Moddable-AP",
	password: "moddable",
	channel: 6,
	onChanged(name) {
		if ("connection" === name) {
			if (this.connection >= 400) {
				trace(`AP up as ${this.SSID} at ${this.address} (${this.MAC}) on channel ${this.channel}\n`);

				this.dnsServer ??= initializeDNSServer();
				this.httpServer ??= initializeHTTPServer();
			}
			else {
				trace(`AP stopped\n`);

				this.dnsServer?.dns();
				delete this.dnsServer;
				this.httpServer?.close();
				delete this.httpServer;
			}
		}
	},
	onConnect(station) {
		this.stations = (this.stations ?? 0) + 1;
		trace(`joined: ${station.MAC}. ${this.stations} connections.\n`);
	},
	onDisconnect(station) {
		this.stations -= 1;
		trace(`left: ${station.MAC}. ${this.stations} connections.\n`);
	}
});

accessPoint.wifi = new WiFi({
	onChanged(property) {
		trace(`wifi ${property} is ${this[property]}\n`);
		this.onChanged?.(property, this[property]);
	}
});

function initializeDNSServer() {
	return new DNSServer({
		socket: {io: UDP},
		onResolve(name) {
			return accessPoint.address;
		}
	});
}

function initializeHTTPServer() {
	return new HTTPServer({
		io: Listener,
		port: 80,
		onConnect(connection) {
			connection.accept({
				onRequest(request) {
					if ("/provision.html" === request.path) {
						this.route = {
							...WebPage,
							data: provisioningPage,
							headers: new Map([["Cache-Control", "no-store"]])
						};
					}
					else if ("/ws" === request.path) {
						this.route = {
							...WebSocketHandshake, 
							onDone() {
								initializeWebSocketConnection(new WebSocket({attach: this.detach()}));
							}
						};
					}
					else {
						this.route = {
							...WebPage,
							data: ArrayBuffer.fromString("Redirect to provision page"),
							status: 301,
							headers: new Map([["location", `http://${accessPoint.address}/provision.html`]])
						}
					}
				}
			})
		}
	});
}

function initializeWebSocketConnection(ws) {
	ws.addEventListener("open", () => {
		ws.send(JSON.stringify({
			event: "initialize"
		}));
	});
	ws.addEventListener("message", event => {
		trace(event.data, "\n");
		const msg = JSON.parse(event.data);
		switch (msg.event) {
			case "scan": {
				const scan = new Map;
				accessPoint.wifi.scan({
					onFound(ap) {
						if (ap.SSID === accessPoint.SSID)
							return;
						const prev = scan.get(ap.SSID);
						if (!prev || (prev.RSSI < ap.RSSI))
							scan.set(ap.SSID, ap);
					},
					onComplete() {
						ws.send(JSON.stringify({
							event: "scan",
							scan: Array.from(scan.values())
						}));
					}
				});
				} break;

			case "connect": {
				const options = {SSID: msg.SSID};
				if (msg.password) options.password = msg.password;
				if (msg.secure) options.secure = msg.secure;
				if (msg.channel) options.channel = msg.channel;
				accessPoint.wifi.connect(options);
				accessPoint.wifi.onChanged = (property, value) => {
					ws.send(JSON.stringify({
						event: "wifi-change",
						property,
						value
					}));
				};
				} break;

			case "disconnect":
				accessPoint.wifi.disconnect();
				break;

			case "terminate":
				accessPoint.httpServer?.close();
				accessPoint.dnsServer?.close();
				accessPoint.wifi?.close();
				accessPoint.close();
				break;
		}
	});
}
