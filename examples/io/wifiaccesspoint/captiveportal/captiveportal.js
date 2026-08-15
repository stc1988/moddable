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

import WebPage from "embedded:network/http/server/route/webpage";
import WebSocketHandshake from "embedded:network/http/server/route/ws/handshake";
import WebSocket from "WebSocket";

const ALPHABET = "abcdefghijkmnpqrstuvwxyz23456789";
function randomText(length) {
	let out = "";
	for (let i = 0; i < length; i++)
		out += ALPHABET[Math.irandom(ALPHABET.length)];
	return out;
}

function clearestChannel(scan) {
	const load = {1: 0, 6: 0, 11: 0};
	for (const ap of scan.values()) {
		let nearest = 1, distance = Infinity;
		for (const c of [1, 6, 11]) {
			if (ap.channel) {
				const d = Math.abs(c - ap.channel);
				if (d < distance) {
					distance = d;
					nearest = c;
				}
			}
		}
		load[nearest] += 1;
	}
	let choice = 1, min = Infinity;
	for (const c of [1, 6, 11]) {
		if (load[c] < min) {
			min = load[c];
			choice = c;
		}
	}
	return choice;
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
		this.#ssid = options.SSID ?? "Moddable";
		this.#password = options.password ?? randomText(10);

		this.#wifi = new WiFi({
			onChanged: property => this.#onWiFiChanged(property, this.#wifi[property])
		});

		this.#setPhase("initializing");
		this.#wifi.scan({
			onFound: ap => {
				if (!ap.SSID) return;
				this.#wifi.found ??= new Map;
				const prev = this.#wifi.found.get(ap.SSID);
				if (!prev || (prev.RSSI < ap.RSSI)) {
					const {BSSID, ...entry} = ap;		// remove BSSID
					this.#wifi.found.set(ap.SSID, entry);
				}
			},
			onComplete: () => {
				const found = this.#wifi.found;
				delete this.#wifi.found;
				if (found)
					this.#scan = Array.from(found.values());

				let SSID;
				do {
					SSID = `${this.#ssid}-${randomText(6)}`;
				} while (found?.has(SSID));

				const options = {
					SSID,
					max: 1,
					onChanged: name => this.#onAPChanged(name),
					onConnect: () => this.#setPhase("connected"),
					onDisconnect: () => {
						this.#ws?.close();
						this.#ws = undefined;
						this.#setPhase("waiting");
					}
				};
				if (this.#channel)
					options.channel = this.#channel;
				else if (found?.size)
					options.channel = clearestChannel(found);
				if (this.#password) options.password = this.#password;
				this.#ap = new WiFiAccessPoint(options);
				this.#ap.configure({
					portal: `http://${this.#ap.address}`
				});
			}
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
			this.#setPhase("ready", {SSID: this.#ap.SSID, password: this.#password});
		}
	}

	#onWiFiChanged(property, value) {
		if ("connection" !== property) return;

		if (value >= 500)
			this.#setPhase("provisioned", this.#credentials);
		else if (value >= 300)
			this.#setPhase("connecting", this.#credentials);
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
		return new device.network.http.server.io({
			...device.network.http.server,
			port: this.#port,
			router: request => {
				if ("GET" !== request.method)
					return;

				if ("/ws" === request.path) {
					return {
						...WebSocketHandshake,
						onDone() {
							portal.#attachWebSocket(new WebSocket({attach: this.detach()}));
						}
					};
				}

				const page = portal.#onPage(request.path);
				if (page) {
					return {
						...WebPage,
						data: page.content,
						headers: new Map([
							["content-type", page.mimeType],
							["cache-control", "no-store"]
						])
					};
				}

				const dest = `http://${portal.#ap.address}`;
				return {
					...WebPage,
					data: ArrayBuffer.fromString(`<HTML><HEAD><META http-equiv="refresh" content="0; URL=${dest}"></HEAD></HTML>`),
					status: 200,
					headers: new Map([
						["location", dest],
						["content-type", "text/html"],
						["cache-control", "no-store"]
					])
				};
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
						const {BSSID, ...entry} = ap;		// remove BSSID
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
		   this.#setPhase("connecting", options);
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
