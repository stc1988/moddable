/*
 * Copyright (c) 2016-2017  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK.
 * 
 *   This work is licensed under the
 *       Creative Commons Attribution 4.0 International License.
 *   To view a copy of this license, visit
 *       <http://creativecommons.org/licenses/by/4.0>.
 *   or send a letter to Creative Commons, PO Box 1866,
 *   Mountain View, CA 94042, USA.
 *
 */

import {} from "piu/MC";

const blackSkin = new Skin({ fill:"black" });
const whiteSkin = new Skin({ fill:"white" });

const buttonsTexture = new Texture({ path:"buttons.png" });

const menuItemSkin = new Skin({ texture:buttonsTexture, x:120, y:0, width:60, height:60, states:60, left:25, right:25, color:"white" });
const menuItemStyle = new Style({ font:"semibold 20px OpenSans", color:["white","#192eabC0"] , horizontal:"center" });
const sliderSkin = new Skin({ fill:"#192eabC0" });
const sliderStyle = new Style({ font:"semibold 20px OpenSans", color:["white","#192eabC0"] , horizontal:"center" });
const sliderCellSkin = new Skin({ fill:["#FFFFFF80", "white"], });
const sliderMinusSkin = new Skin({ texture:buttonsTexture, x:0, y:0, width:60, height:60, states:60, color:"white" });
const sliderPlusSkin = new Skin({ texture:buttonsTexture, x:60, y:0, width:60, height:60, states:60, color:"white" });


let model = {
	get Style() {
		let size = this.size;
		let weight = this.weight;
		let font = weight.values[weight.selection] + " " + size.values[size.selection] + " OpenSans";
		
		let alignment = this.alignment;
		let horizontal = alignment.values[alignment.selection];

		let margins = this.margins;
		margins = parseInt(margins.values[margins.selection]);
		
		let leading = this.leading;
		leading = -parseInt(leading.values[leading.selection]);

		return new Style({ font, horizontal, leading, left:margins, right:margins });
	},
	change() {
		application.purge();
		application.first.style = this.Style;
	},
	alignment: {
		name: "Alignment",
		progress: false,
		selection: 0,
		values: [ "left", "center", "right", "justify" ],
	},
	leading: {
		name: "Leading",
		progress: true,
		selection: 3,
		values: [ "70%", "80%", "90%", "100%", "110%", "120%", "130%" ],
	},
	margins: {
		name: "Margins",
		progress: true,
		selection: 2,
		values: [ "0px", "5px", "10px", "15px", "20px", "25px", "30px" ],
	},
	size: {
		name: "Size",
		progress: true,
		selection: 0,
		values: [ "20px", "22px", "24px", "26px", "28px", "30px", "32px", "34px", "36px", "38px" ],
	},
	weight: {
		name: "Weight",
		progress: false,
		selection: 1,
		values: [ "light", "normal", "semibold", "bold" ],
	},
}

class MenuItemBehavior extends Behavior {
	onCreate(container, data) {
		this.data = data;
	}
	onTap(container) {
		application.replace(application.last, new Slider(this.data));
	}
	onTouchBegan(container) {
		container.state = 1;
	}
	onTouchEnded(container) {
		container.state = 0;
		this.onTap(container);
	}
};

class SliderBehavior extends Behavior {
	onCreate(container, data) {
		this.data = data;
		let minus = container.first;
		let bar = minus.next;
		let plus = bar.next;
		let length = data.values.length, index;
		let width = Math.floor((application.width - minus.width - plus.width - 2) / length) - 2;
		let progress = data.progress;
		let selection = data.selection;
		for (index = 0; index < length; index++) {
			let state = progress ? ((index <= selection) ? 1 : 0) : ((index == selection) ? 1 : 0);
			bar.add(new SliderCell({ width, state }));
		}
		this.min = 0;
		this.max = length - 1;
	}
	onTapCell(container, index) {
		let data = this.data;
		let min = this.min;
		let max = this.max;
		if (index < min)
			index = min;
		else if (index > max)
			index = max;
		if (data.selection != index)
			this.select(container, index);
	}
	onTapMinus(container) {
		let data = this.data;
		let index = data.selection;
		if (index > this.min)
			this.select(container, index - 1);
	}
	onTapPlus(container) {
		let data = this.data;
		let index = data.selection;
		if (index < this.max)
			this.select(container, index + 1);
	}
	select(container, selection) {
		let data = this.data;
		let bar = container.first.next;
		let content = bar.first;
		let index = 0;
		let progress = data.progress;
		while (content) {
			content.state = progress ? ((index <= selection) ? 1 : 0) : ((index == selection) ? 1 : 0);
			content = content.next;
			index++;
		}
		data.selection = selection;
		container.last.string = data.values[selection];
		model.change();
	}
};

class SliderBarBehavior extends Behavior {
	onTouchBegan(container, id, x, y) {
		this.onTouchMoved(container, id, x, y);
	}
	onTouchMoved(container, id, x, y) {
		container.bubble("onTapCell", Math.floor((x - container.x - 2) / container.first.width));
	}
};

class SliderButtonBehavior extends Behavior {
	onTap(container) {
		debugger
	}
	onTouchBegan(container) {
		container.first.state = 1;
	}
	onTouchEnded(container) {
		container.first.state = 0;
		this.onTap(container);
	}
};

class SliderMinusBehavior extends SliderButtonBehavior {
	onTap(container) {
		container.bubble("onTapMinus");
	}
};

class SliderPlusBehavior extends SliderButtonBehavior {
	onTap(container) {
		container.bubble("onTapPlus");
	}
};

let Menu = Container.template($ => ({
	left:0, right:0, top:0, bottom:0, active:true, 
	Behavior: class extends Behavior {
		onTouchEnded(container) {
			application.replace(application.last, new Content({}));
		}
	},
	contents:[
		Row($, {
			bottom:0, style:menuItemStyle, skin:sliderSkin,
			contents: [
				Column($, {
					contents: [
						MenuItem(model.size, { }),
						MenuItem(model.weight, { }),
					]
				}),
				Column($, {
					contents: [
						MenuItem(model.alignment, { }),
						MenuItem(model.leading, { }),
						MenuItem(model.margins, { }),
					]
				}),
			]
		}),
	],
}));

let MenuItem = Label.template($ => ({
	width:application.width / 2, height:60, skin:menuItemSkin, string:$.name, active:true, Behavior:MenuItemBehavior,
}));

let Slider = Container.template($ => ({
	left:0, right:0, height:80, bottom:0, style:sliderStyle, skin:sliderSkin, 
	contents: [
		Container($, { 
			left:0, width:60, top:0, bottom:0, active:true, Behavior:SliderMinusBehavior,
			contents:[
				Content($, { skin:sliderMinusSkin }),
			],
		}),
		Row($, { left:60, right:60, top:0, bottom:0, active:true, Behavior:SliderBarBehavior }),
		Container($, { width:60, right:0, top:0, bottom:0, active:true, Behavior:SliderPlusBehavior,
			contents:[
				Content($, { skin:sliderPlusSkin }),
			],
		}),
		Label($, { left:0, right:0, top:0, height:37, string:$.name }),
		Label($, { left:0, right:0, bottom:0, height:37, string:$.values[$.selection] }),
	],
	Behavior: SliderBehavior,
}));

let SliderCell = Content.template($ => ({
	left:2, width:$.width, height:6, state:$.state, skin:sliderCellSkin,
}));

let TestApplication = Application.template($ => ({
	skin:whiteSkin, active:true, Behavior: class extends Behavior {
		onTouchEnded(container) {
			container.replace(application.last, new Menu());
		}
	},
	contents: [
		Text($, { 
			left:0, right:0, top:0, bottom:0, style:model.Style,
			blocks: [
				{ spans: [
					{ style:new Style({ color:"black" }), spans: "The" },
					{ style:new Style({ color:"navy" }), spans: " quick" },
					{ style:new Style({ color:"maroon" }), spans: " brown" },
					{ style:new Style({ color:"red" }), spans: " fox" },
					{ style:new Style({ color:"purple" }), spans: " jumps" },
					{ style:new Style({ color:"fuchsia" }), spans: " over" },
					{ style:new Style({ color:"green" }), spans: " the" },
					{ style:new Style({ color:"blue" }), spans: " lazy" },
					{ style:new Style({ color:"olive" }), spans: " dog" },
				]},
			]
		}),
		Content($, {}),
	],
	
}));

application = new TestApplication(null, { commandListLength:8192, displayListLength:8192, touchCount:1 });
export default application;
