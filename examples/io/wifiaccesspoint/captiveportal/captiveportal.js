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
import DNSServer from "embedded:network/dns/server/udp";

import Listener from "embedded:io/socket/listener";
import HTTPServer from "embedded:network/http/server";
import WebPage from "embedded:network/http/server/options/webpage";
import WebSocketHandshake from "embedded:network/http/server/options/websocket";
import WebSocket from "WebSocket";

class CaptivePortal {
	#ap;
	#wifi;
	#httpServer;
	#dnsServer;
	#ws;
	#port;
	#onPage;
	#onClose;
	#onError;
	#onStatus;
	#phase = "";
	#credentials;

	constructor(options) {
		const {SSID, onPage, channel, password} = options;
		if (!SSID)
			throw new Error("SSID required");
		if (!onPage)
			throw new Error("onPage required");

		this.#onPage = onPage;
		this.#onClose = options.onClose;
		this.#onError = options.onError;
		this.#onStatus = options.onStatus;
		this.#port = options.port ?? 80;

		options = {
			SSID: SSID,
			max: 1,
			onChanged: name => this.#onAPChanged(name),
			onConnect: station => this.#setPhase("connected"),
			onDisconnect: station => {
				this.#ws?.close();
				this.#ws = undefined;
				this.#setPhase("waiting");
			}
		};
		if (channel) options.channel = channel;
		if (password) options.password = password;
		this.#ap = new WiFiAccessPoint(options);

		this.#wifi = new WiFi({
			onChanged: (property) => this.#onWiFiChanged(property, this.#wifi[property])
		});
	}

	get SSID() {
		return this.#ap?.SSID;
	}

	close() {
		this.#ws?.close();
		this.#httpServer?.close();
		this.#dnsServer?.close();
		this.#wifi?.close();
		this.#ap?.close();
		this.#ws = this.#httpServer = this.#dnsServer = this.#wifi = this.#ap = undefined;
	}

	#onAPChanged(name) {
		if ("connection" !== name) return;
		if (this.#ap.connection >= 400) {
			this.#dnsServer ??= this.#initializeDNSServer();
			this.#httpServer ??= this.#initializeHTTPServer();
			this.#setPhase("waiting");
		}
	}

	#onWiFiChanged(property, value) {
		if ("connection" !== property) return;

		if (value >= 500)
			this.#setPhase("provisioned", this.#credentials);
		else if (300 >= value)
			this.#setPhase("connecting");
		else if ((value <= 200) && ("connecting" === this.#phase))
			this.#setPhase("failed");
	}

	#setPhase(phase, detail) {
		if (this.#phase === phase) return;
		this.#phase = phase;
		this.#onStatus?.(phase, detail);
		this.#sendWS({event: "status", phase});
	}

	#fail(err) {
		this.#onError?.(err);
	}

	#sendWS(msg) {
		try {
			this.#ws?.send(JSON.stringify(msg));
		}
		catch (e) {
			this.#fail(e);
		}
	}

	#initializeDNSServer() {
		return new DNSServer({
			socket: {io: UDP},
			onResolve: () => this.#ap.address
		});
	}

	#initializeHTTPServer() {
		const portal = this;
		return new HTTPServer({
			io: Listener,
			port: this.#port,
			onConnect(connection) {
				connection.accept({
					onRequest(request) {
						if ("/ws" === request.path) {
							this.route = {
								...WebSocketHandshake,
								onDone() {
									portal.#attachWebSocket(new WebSocket({attach: this.detach()}));
								}
							};
							return;
						}

						const page = portal.#onPage(request.path);
						if (page) {
							this.route = {
								...WebPage,
								data: page.content,
								headers: new Map([["cache-control", "no-store"], ["content-type", page.mimeType]])
							};
						}
						else {
							this.route = {
								...WebPage,
								data: ArrayBuffer.fromString(""),
								status: 302,
								headers: new Map([
									["location", "/"],
									["cache-control", "no-store"]
								])
							};
						}
					}
				});
			}
		});
	}

	#attachWebSocket(ws) {
		this.#ws = ws;
		ws.addEventListener("open", () => this.#sendWS({event: "initialize"}));
		ws.addEventListener("message", event => {
			try {
				const msg = JSON.parse(event.data);
				this.#handleWSMessage(msg);
			}
			catch (e) {
				this.#fail(e);
			}
		});
		ws.addEventListener("close", () => {
			this.#ws = null;
		});
		ws.addEventListener("error", () => {
			this.#fail(new Error("WebSocket error"));
		});
	}

	#handleWSMessage(msg) {
		switch (msg.event) {
			case "scan":
				this.#handleScan();
				break;
			case "connect":
				this.#handleConnect(msg);
				break;
			case "disconnect":
				this.#wifi.disconnect();
				break;
			case "terminate":
				this.#handleTerminate();
				break;
		}
	}

	#handleScan() {
		const scan = new Map;
		try {
			this.#wifi.scan({
				onFound: ap => {
					if (ap.SSID === this.#ap.SSID) return;
					const prev = scan.get(ap.SSID);
					if (!prev || (prev.RSSI < ap.RSSI)) {
						const {BSSID, ...entry} = ap;
						scan.set(ap.SSID, entry);
					}
				},
				onComplete: () => {
					this.#sendWS({
						event: "scan",
						scan: Array.from(scan.values())
					});
				}
			});
		}
		catch (e) {
			this.#fail(e);
		}
	}

	#handleConnect(msg) {
		const options = {SSID: msg.SSID};
		if (msg.password) options.password = msg.password;
		if (msg.channel) options.channel = msg.channel;
		if (msg.secure) options.secure = msg.secure;
		this.#credentials = options;
		try {
			this.#wifi.connect(options);
		}
		catch (e) {
			this.#fail(e);
		}
	}

	#handleTerminate() {
		this.close();
		this.#onClose?.();
	}
}

export default CaptivePortal;
