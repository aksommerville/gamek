import { Injector } from "./js/Injector.js";
import { Dom } from "./js/Dom.js";
import { Comm } from "./js/Comm.js";
import { GameController } from "./js/rt/GameController.js";

function loadDemo(name, comm, dom) {
  const wrapper = document.querySelector(".replaceableGameWrapper");
  wrapper.innerHTML = "";
  const controller = dom.spawnController(wrapper, GameController);
  controller.load(`/${name}.wasm`);
}

window.addEventListener("load", () => {
  const injector = new Injector(window, document);
  const dom = injector.getInstance(Dom);
  const comm = injector.getInstance(Comm);
  comm.fetchJson("GET", "/contents.json").then((demoNames) => {
    document.body.innerHTML = "";
    const demoList = dom.spawn(document.body, "UL", ["demoList"]);
    for (const demoName of demoNames) {
      const li = dom.spawn(demoList, "LI");
      const button = dom.spawn(li, "BUTTON", demoName, { "on-click": () => loadDemo(demoName, comm, dom) });
    }
    dom.spawn(document.body, "DIV", ["replaceableGameWrapper"]);
  }).catch(e => console.error(e));
});
