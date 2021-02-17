import pushToTalk from "push-to-talk";
import { stripUnicode } from "./utils/stripUnicode";

type KeyEvent = { key: string; type: string };

(window as any).pushToTalk = {
  start: (callback: (KeyEvent: KeyEvent) => void) => {
    console.log("pushToTalk - start");
    pushToTalk.start((key, isKeyUp) => {
      callback({
        key: stripUnicode(key),
        type: isKeyUp ? "up" : "down",
      });
    });
  },
  stop: () => {
    console.log("pushToTalk - stop");
    pushToTalk.stop();
  },
};
