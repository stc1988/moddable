import config from "mc/config";

export default function (done) {
	if (config.led.rainbow){
		const led = new device.peripheral.led.Default({});
		led.rainbow(3);
	}           

	done?.();
}
