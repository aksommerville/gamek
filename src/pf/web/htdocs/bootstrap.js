import { gamek_init } from "./gamek/gamek.js";

let controller = null;

function showError(message) {
  const element = document.querySelector(".errorMessage");
  if (!element) return;
  if (message) {
    element.innerText = message;
    element.classList.remove("hidden");
  } else {
    element.innerText = "";
    element.classList.add("hidden");
  }
}

function loadDemo(name) {
  const gameContainer = document.querySelector("canvas.gameContainer");
  showError(null);
  window.fetch(`/${name}.wasm`).then(rsp => {
    if (!rsp.ok) throw rsp;
    return rsp.arrayBuffer();
  }).then((bin) => {
  
    document.querySelector("input.hardPause").checked = false;
    for (const element of document.querySelectorAll(".gameButton.running")) {
      element.classList.remove("running");
    }
    let element = document.querySelector(`.gameButton.${name}`);
    if (element) {
      element.classList.add("running");
    }
    
    // Here's the main event, how to run gamek:
    gamek_init(gameContainer, bin).then((ctl) => {
      controller = ctl;
    });
    
  }).catch(e => {
    e.json().then(details => {
      showError(details.log);
    }).catch(() => console.error(e));
  });
}

function hardPause(pause) {
  if (!controller) return;
  if (pause) controller.end();
  else controller.begin();
}
  
/* (args) may contain:
 *  - string => innerText
 *  - array => CSS class names
 *  - {
 *      "on-*" => event listener
 *      "*" => attribute
 *    }
 * Anything else is an error.
 */
function spawn(parent, tagName, ...args) {
  const element = document.createElement(tagName);
  for (const arg of args) {
    switch (typeof(arg)) {
      case "string": element.innerText = arg; break;
      case "object": {
          if (arg instanceof Array) {
            for (const cls of arg) {
              element.classList.add(cls);
            }
          } else for (const k of Object.keys(arg)) {
            if (k.startsWith("on-")) {
              element.addEventListener(k.substr(3), arg[k]);
            } else {
              element.setAttribute(k, arg[k]);
            }
          }
        } break;
      default: throw new Error(`Unexpected argument ${arg}`);
    }
  }
  parent.appendChild(element);
  return element;
}

window.addEventListener("load", () => {
  window.fetch("/contents.json").then(rsp => rsp.json()).then((demoNames) => {
  
    document.body.innerHTML = "";
    
    const controlsRow = spawn(document.body, "DIV", ["controlsRow"]);
    spawn(
      spawn(controlsRow, "LABEL", "Hard pause"),
      "INPUT", ["hardPause"], { type: "checkbox", "on-change": (event) => hardPause(event.target.checked) }
    );
    spawn(controlsRow, "BUTTON", "Fullscreen", { "on-click": () => document.querySelector("canvas.gameContainer")?.requestFullscreen?.() });
    
    const demoList = spawn(document.body, "UL", ["demoList"]);
    for (const demoName of demoNames) {
      const li = spawn(demoList, "LI");
      const button = spawn(li, "BUTTON", ["gameButton", demoName], demoName, { "on-click": () => loadDemo(demoName) });
    }
    spawn(document.body, "CANVAS", ["gameContainer"]);
    
    spawn(document.body, "DIV", ["errorMessage", "hidden"], { "on-click": () => showError(null) });

  }).catch(e => console.error(e));
});
