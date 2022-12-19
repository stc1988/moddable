import Digital from "pins/digital";
import config from "mc/config";
import Button from "button";

class A {
  constructor(options) {
    return new Button({ ...options, invert: true, pin: 7 });
  }
}

globalThis.Host = Object.freeze(
  {
    LED: {
      Default: class {
        #pin = {
          white: new Digital(25, Digital.Output),
        };
        constructor() {
          this.write({ white: 1 });
        }
        close() {
          this.write({ white: 1 });
          this.#pin.white.close();
        }
        write(on) {
          this.#pin.white.write(on ? 0 : 1);
        }
      },
    },
    Button: {
      Default: A,
      A,
      B: class {
        constructor(options) {
          return new Button({ ...options, invert: true, pin: 8 });
        }
      },
      C: class {
        constructor(options) {
          return new Button({ ...options, invert: true, pin: 9 });
        }
      },
      UP: class {
        constructor(options) {
          return new Button({ ...options, invert: true, pin: 22 });
        }
      },
      DOWN: class {
        constructor(options) {
          return new Button({ ...options, invert: true, pin: 6 });
        }
      },
    },
  },
  true
);

export default function (done) {
  done();
}
