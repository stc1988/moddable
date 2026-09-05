import "system"		// system initializes globalThis.device. this ensures it runs before this module.

import HTTPServer from "embedded:network/http/server";
import Listener from "embedded:io/socket/listener";

globalThis.device = Object.freeze({
	...globalThis.device,
	network: {
		...globalThis.device?.network,
		http: {
			...globalThis.device?.network?.http,		// client might be here already
			server: {
				io: HTTPServer,
				socket: {
					io: Listener
				}
			}
		}
	},
}, true);
