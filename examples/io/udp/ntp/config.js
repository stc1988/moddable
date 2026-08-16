import "dns/config";

import UDP from "embedded:io/socket/udp";		// the NTP client's own transport

import NTP from "embedded:network/ntp/client";

const dns = device.network.dns.resolver;
globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		ntp: {
			client: {
				io: NTP,
				dns,
				socket: {
					io: UDP
				},
				servers: ["pool.ntp.org"]
			}
		}
	},
}, true);
