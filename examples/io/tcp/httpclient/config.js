import "dns/config";

import TCP from "embedded:io/socket/tcp";

import HTTPClient from "embedded:network/http/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		http: {
			...globalThis.device?.network?.http,		// server might be here already
			client: {
				io: HTTPClient,
				dns,
				socket: {
					io: TCP
				}
			},
			//@@ compatibility - REMOVE
			io: HTTPClient,
			dns,
			socket: {
				io: TCP
			},
		}
	},
}, true);
