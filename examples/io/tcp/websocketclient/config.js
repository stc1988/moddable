import "dns/config";

import TCP from "embedded:io/socket/tcp";

import WebSocketClient from "embedded:network/websocket/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		ws: {
			io: WebSocketClient,
			dns,
			socket: {
				io: TCP,
			},		
		},
	},
}, true);

