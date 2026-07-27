import { WebSocketServer } from 'ws';
import fs from 'fs';
import http from 'http';

const server = http.createServer((req, res) => {
// 	console.error(req.method, req.url);
	const names = req.url.split("/");
  	const fileName = names.at(-1);
	if (req.method === 'GET') {
		if (fileName === 'index.html') {
			fs.readFile('index.html', (err, data) => {
				res.writeHead(200, { 'Content-Type': 'text/html' });
				res.end(data);
			});
		} 
		else if (fileName === 'favicon.ico') {
			fs.readFile('favicon.ico', (err, data) => {
				res.writeHead(200, { 'Content-Type': 'image/x-icon' });
				res.end(data);
			});
		} 
		else if (fileName === 'device.png') {
			fs.readFile('device.png', (err, data) => {
				res.writeHead(200, { 'Content-Type': 'image/png' });
				res.end(data);
			});
		} 
		else if (fileName === 'fingerprint.png') {
			fs.readFile('fingerprint.png', (err, data) => {
				res.writeHead(200, { 'Content-Type': 'image/png' });
				res.end(data);
			});
		} 
		else {
			res.writeHead(404, { 'Content-Type': 'text/html' });
			res.end('<h1>404 Not Found ${ req.url }</h1>');
		}
	}
	else {
		res.writeHead(405, { 'Content-Type': 'text/html' });
		res.end('<h1>405 Method Not Allowed</h1>');
	}
});
server.on('clientError', (err, socket) => {
  socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
});

const ws = new WebSocketServer({ server, clientTracking:true });
let receiver = null;
ws.on('connection', (socket, request, client) => {
	if (request.url == "/") {
		console.error('WebSocket sender connected');
		socket.on('message', (message, isBinary) => {
			if (receiver)
				receiver.send(isBinary ? message : message.toString());
		});
		socket.on('close', () => {
			console.log('WebSocket sender disconnected');
		});
    }
    else if (request.url == "/get") {
		console.error('WebSocket receiver connected');
    	receiver = socket;
		socket.on('close', () => {
			receiver = null;
			console.log('WebSocket receiver disconnected');
		});
    }
    else if (request.url == "/echo") {
		console.error('WebSocket echo connected');
		socket.on('message', (message, isBinary) => {
			socket.send(isBinary ? message : message.toString());
		});
		socket.on('close', () => {
			console.log('WebSocket echo disconnected');
		});
    }
});
ws.on('error', (error) => {
	console.error(error);
});
ws.on('listening', () => {
	console.error('WebSocket server listening', ws.address());
});

server.listen(8081, () => {
  console.error('HTTP server listening', server.address());
});
