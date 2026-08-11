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

/*
	Simulated Captive Portal.

	Presents the same interface as the captive portal, and takes a project through the
	same sequence of phases, without any networking: there is no HTTP server, no DNS
	server, and no WebSocket. A ghost client stands in for the phone that would join
	the access point and work through the provisioning page.

	The ghost asks for the pages the real portal would serve, so a project's onPage()
	is called with the paths a browser would request, and reports anything it could not
	have served to onError(). It then scans, chooses a network, and provisions, which
	drives the connecting / provisioned phases through the simulated Wi-Fi station.
*/

import WiFiAccessPoint from "embedded:network/interface/wifi/accesspoint";
import WiFi from "embedded:network/interface/wifi";
import Timer from "timer";

const kStep = 300;					// pause between the ghost client's steps
const kGhostPassword = "simulated";	// what the ghost client types into the provisioning page

const ALPHABET = "abcdefghijkmnpqrstuvwxyz23456789";
function randomText(length) {
	let out = "";
	for (let i = 0; i < length; i++)
		out += ALPHABET[Math.irandom(ALPHABET.length)];
	return out;
}

class CaptivePortal {
	#ap;
	#wifi;
	#onPage;
	#onClose;
	#onError;
	#onStatus;
	#phase = "";
	#credentials;
	#ssid;
	#password;
	#scan;
	#steps;
	#timer;
	#closed;

	constructor(options) {
		const {onPage} = options;
		if (!onPage)
			throw new Error("onPage required");

		this.#onPage = onPage;
		this.#onClose = options.onClose;
		this.#onError = options.onError;
		this.#onStatus = options.onStatus;
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
				if (this.#closed)
					return;

				const found = this.#wifi.found;
				delete this.#wifi.found;
				if (found)
					this.#scan = Array.from(found.values());

				let SSID;
				do {
					SSID = `${this.#ssid}-${randomText(6)}`;
				} while (found?.has(SSID));

				const apOptions = {
					SSID,
					max: 1,
					onChanged: name => this.#onAPChanged(name),
					onConnect: () => this.#onGhostJoined(),
					onDisconnect: () => this.#setPhase("waiting")
				};
				if (options.channel)
					apOptions.channel = options.channel;
				if (this.#password)
					apOptions.password = this.#password;

				this.#ap = new WiFiAccessPoint(apOptions);
				this.#ap.configure({portal: `http://${this.#ap.address}`});
			}
		});
	}

	get SSID() {
		return this.#ap?.SSID;
	}

	close() {
		this.#closed = true;

		Timer.clear(this.#timer);
		this.#timer = undefined;

		this.#wifi?.close();
		this.#ap?.close();
		this.#wifi = this.#ap = undefined;
	}

	#onAPChanged(name) {
		if ("connection" !== name) return;
		if (this.#ap.connection >= 400)
			this.#setPhase("ready", {SSID: this.#ap.SSID, password: this.#password});
	}

	#onWiFiChanged(property, value) {
		if ("connection" !== property) return;

		if (value >= 500) {
			this.#setPhase("provisioned", this.#credentials);
			this.#next();				// provisioned, so the ghost is done
		}
		else if (value >= 300)
			this.#setPhase("connecting", this.#credentials);
		else if ((value <= 200) && ("connecting" === this.#phase)) {
			this.#setPhase("failed");
			this.#next();
		}
	}

	#setPhase(phase, detail) {
		if (this.#phase === phase) return;
		this.#phase = phase;
		this.#onStatus?.(phase, detail);
	}

	#fail(err) {
		this.#onError?.(err);
	}

	/*
		The ghost client. Each step is what the provisioning page would have done over
		HTTP and the WebSocket, in the order a browser does it.
	*/
	#onGhostJoined() {
		this.#setPhase("connected");
		this.#steps = [
			() => this.#request("/"),				// the browser opens the portal
			() => this.#request("/no-such-page"),	// and something that must fall through to the redirect
			() => this.#requestScan(),				// "scan" over the WebSocket
			() => this.#requestConnect(),			// "connect" with the chosen credentials
			() => this.#requestTerminate()			// "terminate" once provisioned
		];
		this.#next();
	}
	// a step returns false when it continues on its own, and calls #next() when it is done
	#next() {
		if (this.#closed || !this.#steps?.length)
			return;

		Timer.clear(this.#timer);
		this.#timer = Timer.set(() => {
			this.#timer = undefined;
			if (this.#closed)
				return;

			const step = this.#steps.shift();
			if (false !== step())
				this.#next();
		}, kStep);
	}
	// what the HTTP server would have served for one request
	#request(path) {
		let page;
		try {
			page = this.#onPage(path);
		}
		catch (e) {
			this.#fail(e);
			return;
		}

		if (!page) {
			if ("/" === path)		// the portal cannot serve its own home page
				this.#fail(new Error(`no page for "${path}"`));
			return;					// otherwise the real portal redirects to itself
		}

		if (!page.content)
			this.#fail(new Error(`page for "${path}" has no content`));
		if (!page.mimeType)
			this.#fail(new Error(`page for "${path}" has no mimeType`));
	}
	#requestScan() {
		if (this.#scan)				// the scan from start up, as the real portal reuses it
			return;

		try {
			const scan = new Map;
			this.#wifi.scan({
				onFound: ap => {
					if (ap.SSID === this.#ap?.SSID) return;
					const prev = scan.get(ap.SSID);
					if (!prev || (prev.RSSI < ap.RSSI))
						scan.set(ap.SSID, ap);
				},
				onComplete: () => {
					if (this.#closed)
						return;
					this.#scan = Array.from(scan.values());
					this.#next();
				}
			});
			return false;
		}
		catch (e) {
			this.#fail(e);
		}
	}
	// the ghost picks the strongest network it found, as someone would from the list
	#requestConnect() {
		let choice;
		for (let ap of this.#scan ?? []) {
			if (!choice || (ap.RSSI > choice.RSSI))
				choice = ap;
		}
		if (!choice) {
			this.#fail(new Error("no network to provision"));
			return;
		}

		const credentials = {SSID: choice.SSID};
		if ("none" !== choice.security) {
			credentials.password = kGhostPassword;
			credentials.secure = true;
		}
		this.#credentials = credentials;

		try {
			this.#wifi.connect(credentials);
			this.#setPhase("connecting", credentials);
			return false;			// #onWiFiChanged continues once provisioned or failed
		}
		catch (e) {
			this.#fail(e);
		}
	}
	#requestTerminate() {
		this.close();
		this.#onClose?.();
	}
}

export default CaptivePortal;
