import ILI9341 from "ili9341";
import Digital from "embedded:io/digital";

export default class extends ILI9341 {
	constructor(dictionary) {
		// Detect IPS LCD (externally pulled up) vs TN on TFT_RST (GPIO 33)
		let pin33 = new Digital({
			pin: 33,
			mode: Digital.InputPullDown
		});
		const isIps = pin33.read();
		pin33.close();
		pin33 = new Digital({
			pin: 33,
			mode: Digital.Output
		});
		pin33.write(1);
		super(dictionary);
		if (isIps)
			super.command(0x21);	// invert
	}
}
