import { gamek_init } from "./gamek/gamek.js";
import { gamekInputConfigModal } from "./gamekInputConfigModal.js";

let controller = null;
let editInputInProgress = null; // 'resolve' hook if modal up

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

function showToast(message) {
  const element = document.querySelector(".toast");
  if (!element) return;
  element.innerText = message;
  element.classList.remove("hidden");
  window.setTimeout(() => element.classList.add("hidden"), 5000);
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
      try {
        const inputConfig = JSON.parse(window.localStorage.getItem("gamekInputConfig"));
        if (inputConfig && (typeof(inputConfig) === "object")) {
          controller.input.setConfiguration(inputConfig);
        }
      } catch (e) {}
      controller.input.onKillTouchEvents = () => showToast("Touch events disabled. Pause/resume to reenable.");
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

function configureInput() {
  let original;
  if (controller) {
    original = controller.input.getConfiguration();
  } else {
    try {
      original = JSON.parse(window.localStorage.getItem("gamekInputConfig"));
    } catch (e) {
      original = {};
    }
  }
  if (!original || (typeof(original) !== "object")) original = {};
  gamekInputConfigModal(original, controller).then((edited) => {
    if (edited) {
      if (controller) {
        controller.input.setConfiguration(edited);
      }
      window.localStorage.setItem("gamekInputConfig", JSON.stringify(edited));
    }
  }).catch((e) => {
    console.error(`...error from input modal`, e);
  });
}

function enterFullscreen() {
  if (!controller) return;
  document.querySelector("canvas.gameContainer")?.requestFullscreen?.();
  // Toggle pause real quick, to force re-enable touch events, and maybe other transient things.
  controller.end();
  controller.begin();
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
    spawn(controlsRow, "BUTTON", "Fullscreen", { "on-click": () => enterFullscreen() });
    spawn(controlsRow, "BUTTON", "Input...", { "on-click": () => configureInput() });
    
    const demoList = spawn(document.body, "UL", ["demoList"]);
    for (const demoName of demoNames) {
      const li = spawn(demoList, "LI");
      const button = spawn(li, "BUTTON", ["gameButton", demoName], demoName, { "on-click": () => loadDemo(demoName) });
    }
    spawn(document.body, "CANVAS", ["gameContainer"]);
    
    spawn(document.body, "DIV", ["errorMessage", "hidden"], { "on-click": () => showError(null) });
    
    spawn(document.body, "DIV", ["toast", "hidden"]);

  }).catch(e => console.error(e));
});

window.addEventListener("visibilitychange", (event) => {
  if (document.hidden) {
    const checkbox = document.querySelector("input.hardPause");
    if (checkbox) {
      checkbox.checked = true;
      checkbox.dispatchEvent(new Event("change"));
    }
  }
});
