import "dns/config";

import TCP from "embedded:io/socket/tcp";
import TLSSocket from "embedded:io/socket/tcp/tls";

import MQTTClient from "embedded:network/mqtt/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		mqtts: {
			io: MQTTClient,
			dns,
			socket: {
				io: TLSSocket,
				TCP: {
					io: TCP
				},
				tls: {
					applicationLayerProtocol: "mqtt"
				}
			},
		},
	},
}, true);
