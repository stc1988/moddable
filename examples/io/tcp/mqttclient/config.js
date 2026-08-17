import "dns/config";

import TCP from "embedded:io/socket/tcp";

import MQTTClient from "embedded:network/mqtt/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		mqtt: {
			io: MQTTClient,
			dns,
			socket: {
				io: TCP,
			},		
		},
	},
}, true);

