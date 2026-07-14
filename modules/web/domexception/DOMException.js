class DOMException extends Error {
	constructor(message, name) {
		super(message);
		Object.defineProperty(this, "name", { value:name });
	}
}

export { 
	DOMException,
} 
