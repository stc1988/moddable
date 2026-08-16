import Timer from "timer";

class System {
}
System.setInterval = Timer.repeat;
System.clearInterval = Timer.clear;

globalThis.System = System;
