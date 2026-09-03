import Resource from "Resource";

import {Outline} from "commodetto/outline";
import {} from "piu/MC";
import {} from "piu/shape";
import Timeline from "piu/Timeline";

const TRANSPARENT = "transparent";
const WHITE = "white";

const backgroundSkin = new Skin({ fill:"#373636" });
const whiteSkin = new Skin({ fill:"#62626280", stroke:"#626262" });

const ascent = 60;
const descent = 20;
const fonts = [
	new Resource("OpenSans-Regular.ttf"),
	new Resource("Radley-Italic.ttf"),
	new Resource("SourceCodePro-Regular.ttf"),
];
fonts[0].size = 72;
fonts[1].size = 80;
fonts[2].size = 72;

const shapeSkins = [
	new Skin({ fill:[TRANSPARENT,"#5f9dfb",WHITE] }),
	new Skin({ fill:[TRANSPARENT,"#71b4ff",WHITE] }),
	new Skin({ fill:[TRANSPARENT,"#63b7cd",WHITE] }),
	new Skin({ fill:[TRANSPARENT,"#71fef8",WHITE] }),
	new Skin({ fill:[TRANSPARENT,"#67dda1",WHITE] }),
];

const digits = [
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten",
	"eleven",
	"twelve",
	"thirteen",
	"fourteen",
	"fifteen",
	"sixteen",
	"seventeen",
	"eighteen",
	"nineteen",
	"twenty",
	"twenty-one",
	"twenty-two",
	"twenty-three",
	"twenty-four",
	"twenty-five",
	"twenty-six",
	"twenty-seven",
	"twenty-eight",
	"twenty-nine",
	"thirty",
	"thirty-one",
	"thirty-two",
	"thirty-three",
	"thirty-four",
	"thirty-five",
	"thirty-six",
	"thirty-seven",
	"thirty-eight",
	"thirty-nine",
	"forty",
	"forty-one",
	"forty-two",
	"forty-three",
	"forty-four",
	"forty-five",
	"forty-six",
	"forty-seven",
	"forty-eight",
	"forty-nine",
	"fifty",
	"fifty-one",
	"fifty-two",
	"fifty-three",
	"fifty-four",
	"fifty-five",
	"fifty-six",
	"fifty-seven",
	"fifty-eight",
	"fifty-nine",
]

class ApplicationBehavior extends Behavior {
	constructor() {
		super();
		this.hours = -1;
		this.minutes = -1;
	}
	onDisplaying(application) {
		this.onTimeChanged(application);
		application.start();
	}
	onTimeChanged(application) {
		const date = new Date();
		let hours = date.getHours();
		let minutes = date.getMinutes();
		let seconds = date.getSeconds();
		let milliseconds = date.getMilliseconds();
		if ((this.hours == hours) && (this.minutes == minutes)) {
			application.distribute("onChanged", seconds, milliseconds);
			return;
		}
		this.hours = hours;
		this.minutes = minutes;
		
		let words = ["it's"];
		if (minutes == 0) {
		}
		else if (minutes == 15) {
			words.push("a quarter");
			words.push("past");
		}
		else if (minutes < 30) {
			words.push(digits[minutes]);
			if (minutes == 1) {
				words.push("minute");
			}
			else if (minutes % 5) {
				words.push("minutes");
			}
			words.push("past");
		}
		else if (minutes == 30) {
			words.push("half");
			words.push("past");
		}
		else if (minutes == 45) {
			words.push("a quarter");
			words.push("to");
			hours++;
		}
		else {
			minutes = 60 - minutes;
			words.push(digits[minutes]);
			if (minutes == 1) {
				words.push("minute");
			}
			else if (minutes % 5) {
				words.push("minutes");
			}
			words.push("to");
			hours++;
		}
		hours %=24;
		if (hours == 0) {
			words.push("midnight");
		}
		else if (hours == 12) {
			words.push("noon");
		}
		else {
			words.push(digits[hours % 12]);
			if (minutes == 0) {
				words.push("o'clock");
			}
		}
		this.words = words.map((word, index) => {
			const font = fonts[index % 3];
			const fillPath = Outline.TextPath(font, word);
			const fillOutline = Outline.fill(fillPath);
			const bounds = fillOutline.bounds;
			fillOutline.translate(-bounds.x, ascent)
			const width = bounds.width;
			const height = ascent + descent;
			return { fillOutline, width, height };
		});
		application.distribute("onChanged", 60, 0);
		application.replace(application.first, new ClockContainer(this.words));
		application.purge();
	}
}

class ClockContainerBehavior extends Behavior {
	onCreate(container, $) {
		let shape = container.first.first;
		let count = 0;
		while (shape) {
			const behavior = shape.behavior;
			shape.skin = shapeSkins[count];
			shape = shape.next;
			count++;
		}
	}
	onChanged(container, seconds, milliseconds) {
		const time = (seconds * 1000) + milliseconds;
		const fromTime = this.fromTimeline.duration;
		const repeatTime = this.repeatTimeline.duration;
		const toTime = 60000 - this.toTimeline.duration;
		
		if (time < fromTime)
			this.fromTimeline.seekTo(time);
		else if (time < toTime)
			this.repeatTimeline.seekTo((time - fromTime) % repeatTime);
		else
			this.toTimeline.seekTo(time - toTime);
			
		container.distribute("onStep", time / 60000);
	}
	onDisplaying(container) {
		const fromTimeline = new Timeline;
		const repeatTimeline = new Timeline;
		const toTimeline = new Timeline;
		let delay, duration, shape;
		
		duration = 500;
		shape = container.first.first;
		while (shape) {
			fromTimeline.from(shape.behavior, { scale:2 }, duration, Math.quadEaseOut, 0);
			fromTimeline.from(shape, { state:0 }, duration / 2, Math.quadEaseOut, -duration);
			shape = shape.next;
		}
		
		delay = 0;
		duration = 1000;
		shape = container.first.first;
		while (shape) {
			repeatTimeline.on(shape.behavior, { angle:[0, -2 * Math.PI], ry:[-shape.y - 160] }, duration, Math.quadEaseOut, delay);
			delay = -800;
			shape = shape.next;
		}

		duration = 500;
		shape = container.first.first;
		while (shape) {
			repeatTimeline.on(shape.behavior, { scale:[0.5, 0.25, 1, 0.5] }, duration, Math.quadEaseOut, 0);
			repeatTimeline.on(shape, { state:[1, 2, 2, 1] }, duration, Math.quadEaseOut, -duration);
			shape = shape.next;
		}
		
		delay = 0;
		duration = 1000;
		shape = container.first.last;
		while (shape) {
			repeatTimeline.on(shape.behavior, { angle:[ 2 * Math.PI, 0], ry:[480 - shape.y] }, duration, Math.quadEaseOut, delay);
			delay = -800;
			shape = shape.previous;
		}
		
		delay = 0;
		duration = 500;
		shape = container.first.last;
		while (shape) {
			repeatTimeline.on(shape.behavior, { scale:[0.5, 0.25, 1, 0.5] }, duration, Math.quadEaseOut, 0);
			repeatTimeline.on(shape, { state:[1, 2, 2, 1] }, duration, Math.quadEaseOut, -duration);
			shape = shape.previous;
		}
		
		delay = 0;
		duration = 500;
		shape = container.first.first;
		while (shape) {
			toTimeline.to(shape, { state:0 }, duration, Math.quadEaseOut, delay);
			delay = -duration;
			shape = shape.next;
		}
		this.fromTimeline = fromTimeline;
		this.repeatTimeline = repeatTimeline;
		this.toTimeline = toTimeline;
		this.onChanged(container, 0, 0);
	}
}

class ClockShapeBehavior extends Behavior {
	onCreate(shape, $) {
		this.fillOutline = $.fillOutline;
		this.cx = $.width / 2;
		this.cy = $.height / 2;
		this.angle = 0;
		this.rx = 0;
		this.ry = 0;
		this.scale = 0.5;
	}
	onStep(shape) {
		const cx = this.cx;
		const cy = this.cy;
		shape.fillOutline = this.fillOutline.clone().translate(-cx, -cy).rotate(this.angle, this.rx, this.ry).scale(this.scale).translate(cx / 2, cy / 2);
	}
}

let ClockApplication = Application.template($ => ({
	behavior:$, skin:backgroundSkin,
	contents: [
		Content($, {}),
		TimeShape($),
	],
}));

let ClockContainer = Container.template($ => ({
	Behavior:ClockContainerBehavior, left:0, right:0, top:0, bottom:0, skin:backgroundSkin,
	contents: [
		ClockColumn($),
	],
}));

let ClockColumn = Column.template($ => ({
	contents: [
		$.map(($$, index) => new ClockShape($$))
	],
}));

let ClockShape = Shape.template($ => ({
	Behavior:ClockShapeBehavior, width:$.width >> 1, height:$.height >> 1, state:1, clip:false,
}));

class TimeShapeBehavior extends Behavior {
	onCreate(shape) {
		this.seconds = -1;
	}
	onChanged(shape, seconds) {
		if (this.seconds != seconds) {
			this.seconds = seconds;
			const path = new Outline.CanvasPath;
			const x = shape.width >> 1;
			const y = shape.height >> 1;
			const from = -90;
			const to = -90 + (6 * seconds);
			path.moveTo(x, y);
			path.arc(x, y, y - 8, from * Math.PI / 180, to * Math.PI / 180);
			path.lineTo(x, y);
			shape.strokeOutline = Outline.fill(path);
			shape.time = 0;
			shape.start();
		}
	}
	onDisplaying(shape) {
		const x = shape.width >> 1;
		const y = shape.height >> 1;
		const path = new Outline.CanvasPath;
		path.arc(x, y, y - 8, 0, 2 * Math.PI);
		shape.fillOutline = Outline.fill(path);
	}
}

let TimeShape = Shape.template($ => ({
	Behavior:TimeShapeBehavior, left:0, right:0, height:40, bottom:0, skin:whiteSkin,
}));

screen.frameRate = 75;

export default new ClockApplication(new ApplicationBehavior(), 
	{
		commandListLength: 2048,
		displayListLength: 2048,
		pixels: 32 * screen.width,
		touchCount: 1,
	}
);
