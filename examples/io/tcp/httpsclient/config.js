import TLSSocket from "embedded:io/socket/tcp/tls";
import "httpclient/config";

globalThis.device = {
	...globalThis.device,
	network: {
		...globalThis.device.network
	}
};

const socket = {
	io: TLSSocket,
	TCP: device.network.http.socket,
	tls: {
		applicationLayerProtocol: "http/1.1"
	}
};

device.network.https = {
	client: {
		io: device.network.http.io,
		dns: device.network.http.dns,
		port: 443,
		socket
	},
	//@@ compatibilty - REMOVE
	io: device.network.http.io,
	dns: device.network.http.dns,
	port: 443,
	socket
};
Object.freeze(device, true);
