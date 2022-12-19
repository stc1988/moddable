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



let message = "Hello, world - sample";
trace(message + "\n");

const Host = globalThis.Host;

new Host.Button.Default({
    onPush() {
        trace(`${this.pressed}\n`);
    }
});

var buttonA =new Host.Button.A({
  onPush() {
    trace(`${this.pressed}\n`);
  }
});
var buttonB =new Host.Button.B({
  onPush() {
    trace(`${this.pressed}\n`);
  }
});
// var buttonC=new Host.Button.C({
//   onPush() {
//     trace(`${this.pressed}\n`);
//   }
// });
