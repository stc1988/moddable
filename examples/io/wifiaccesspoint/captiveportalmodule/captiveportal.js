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

const ALPHABET = "abcdefghijkmnpqrstuvwxyz23456789";		// unambiguous; QR-safe
function randomText(length) {
	let out = "";
	for (let i = 0; i < length; i++)
		out += ALPHABET[(Math.random() * ALPHABET.length) | 0];
	return out;
}

class CaptivePortal {
	#ap;
	#wifi;
	#httpServer;
	#dnsServer;
	#ws;
	#port;
	#channel;
	#onPage;
	#onClose;
	#onError;
	#onStatus;
	#phase = "";
	#credentials;
	#ssid;
	#ssidPrefix;
	#password;
	#scan;

	constructor(options) {
		const {onPage} = options;
		if (!onPage)
			throw new Error("onPage required");

		this.#onPage = onPage;
		this.#onClose = options.onClose;
		this.#onError = options.onError;
		this.#onStatus = options.onStatus;
		this.#port = options.port ?? 80;
		this.#channel = options.channel;
		this.#ssid = options.SSID;						// used as-is when provided
		this.#ssidPrefix = options.SSIDPrefix ?? "Moddable-";
		this.#password = options.password;				// generated when absent

		this.#wifi = new WiFi({
			onChanged: property => this.#onWiFiChanged(property, this.#wifi[property])
		});
		this.#prescan();
	}

	get SSID() {
		return this.#ap?.SSID;
	}

	#prescan() {
		this.#setPhase("initializing");
		const found = new Map;
		try {
			this.#wifi.scan({
				onFound: ap => {
					if (!ap.SSID) return;
					const prev = found.get(ap.SSID);
					if (!prev || (prev.RSSI < ap.RSSI)) {
						const {BSSID, ...entry} = ap;
						found.set(ap.SSID, entry);
					}
				},
				onComplete: () => this.#startAccessPoint(found)
			});
		}
		catch (e) {
			this.#fail(e);
		}
	}

	#startAccessPoint(found) {
		this.#scan = Array.from(found.values());		// cached for the portal page

		if (!this.#ssid) {
			do {
				this.#ssid = this.#ssidPrefix + randomText(6);
			} while (found.has(this.#ssid));
		}
		this.#password ??= randomText(10);

		const options = {
			SSID: this.#ssid,
			max: 4,
			onChanged: name => this.#onAPChanged(name),
			onConnect: station => this.#setPhase("connected"),
			onDisconnect: station => {
				this.#ws?.close();
				this.#ws = undefined;
				this.#setPhase("waiting");
			}
		};
		if (this.#channel) options.channel = this.#channel;
		if (this.#password) options.password = this.#password;
		this.#ap = new WiFiAccessPoint(options);
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
			// AP is up: report the finalized credentials so the caller can show
			// them (e.g. render the QR). Subsequent no-station states use "waiting".
			this.#setPhase("ready", {SSID: this.#ssid, password: this.#password});
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
								headers: new Map([
									["content-type", page.mimeType],
									["cache-control", "no-store"],
									["connection", "close"]
								])
							};
						}
						else {
							this.route = {
								...WebPage,
								data: ArrayBuffer.fromString("redirecting to captive portal"),
								status: 302,
								headers: new Map([
									["location", `http://${portal.#ap.address}/`],
									["content-type", "text/plain"],
									["cache-control", "no-store"],
									["connection", "close"]
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
		ws.addEventListener("open", () => this.#sendWS({event: "ws initialize"}));
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
		if (this.#scan) {
			this.#sendWS({event: "scan", scan: this.#scan});
			this.#scan = undefined;
			return;
		}

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
