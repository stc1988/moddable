import "dns/config";

import TLSSocket from "embedded:io/socket/tcp/tls";

import WebSocketClient from "embedded:network/websocket/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device.network,
		wss: {
			io: WebSocketClient,
			dns,
			socket: {
				io: TLSSocket,
				TCP: device.network.ws.socket,
				tls: {
					applicationLayerProtocol: "http/1.1"	// handshake is HTTP/1.1 upgrade
				}
			}
		},
	},
}, true);
