import "system"		// system initializes globalThis.device. this ensures it runs before this module.

import UDP from "embedded:io/socket/udp";
import Resolver from "embedded:network/dns/resolver/udp";

globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		dns: {
			...globalThis.device?.network?.dns,
			resolver: {
				io: Resolver,
				servers: ["1.1.1.1", "8.8.8.8"],
				socket: {
					io: UDP
				}
			}
		}
	},
}, true);
