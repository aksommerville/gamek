/* gamekInputConfigModal.js
 * Helper to present an interactive modal for input config.
 * This is pretty opinionated, so I'm not including in the core package.
 * See our bootstrap.js for usage, not much to it.
 */
 
/* Attach a modal to document.body, and let the user configure input.
 * Returns a Promise that resolves on dismiss, with either null or a new input configuration.
 * These can be piped straight into GamekInput.setConfiguration/getConfiguration.
 * (controller) is optional. If present, we attach a raw input listener.
 */
export function gamekInputConfigModal(original, controller) {
  const container = createInputConfigModalUi();
  populateUi(container, original);
  let listener = null;
  if (controller && controller.input) {
    listener = attachInputListener(container, controller.input);
  }
  return awaitDismissal(container).then((content) => {
    if (listener) controller.input.unlisten(listener);
    container.remove();
    return content;
  });
}

/* Private.
 ************************************************************************************************/
 
/* Create UI.
 */

function createInputConfigModalUi() {

  const container = document.createElement("FORM");
  container.classList.add("gamekInputConfigModal");
  container.onsubmit = (event) => submitInputConfigModal(container, event);
  
  const body = document.createElement("DIV");
  container.appendChild(body);
  body.classList.add("body");

  const deviceList = document.createElement("UL");
  body.appendChild(deviceList);
  deviceList.classList.add("deviceList");
  
  const deviceForm = document.createElement("DIV");
  body.appendChild(deviceForm);
  deviceForm.classList.add("deviceForm");
  
  const dismissalRow = document.createElement("DIV");
  container.appendChild(dismissalRow);
  dismissalRow.classList.add("dismissalRow");
  
  const okButton = document.createElement("INPUT");
  dismissalRow.appendChild(okButton);
  okButton.type = "submit";
  okButton.value = "OK";
  
  const cancelButton = document.createElement("INPUT");
  dismissalRow.appendChild(cancelButton);
  cancelButton.type = "button";
  cancelButton.value = "Cancel";
  cancelButton.onclick = () => dismissInputConfigModal(container);
  
  const keyNameList = document.createElement("DATALIST");
  keyNameList.id = "gamekInputConfigModal-keyboard-keys";
  for (const code of keyNames) {
    const option = document.createElement("OPTION");
    option.value = code;
    keyNameList.appendChild(option);
  }
  container.appendChild(keyNameList);
  
  container.addEventListener("change", (event) => onInputConfigChange(container, event));
  
  document.body.appendChild(container);
  return container;
}

/* Populate UI.
 */

function populateUi(container, config) {
  if (!config) config = {};
  if (!config.keyboard) config.keyboard = [];
  if (!config.gamepads) config.gamepads = [];
  container._gamekInputConfigOriginal = config;
  container._gamekInputConfigContent = JSON.parse(JSON.stringify(config));
  
  const deviceList = container.querySelector(".deviceList");
  deviceList.innerHTML = "";
  addDeviceToList(deviceList, "Keyboard", () => showKeyboard(container));
  addDeviceToList(deviceList, "Touch", () => showTouch(container));
  addDeviceToList(deviceList, "MIDI", () => showMidi(container));
  if (config && config.gamepads) {
    for (const gamepad of config.gamepads) {
      addDeviceToList(deviceList, gamepad.id, () => showGamepad(container, gamepad));
    }
  }
  addDeviceToList(deviceList, "Add Gamepad", () => addGamepad(container));
}

function addDeviceToList(deviceList, name, cb) {
  const li = document.createElement("LI");
  deviceList.appendChild(li);
  li.innerText = name;
  li.onclick = () => {
    if (name !== "Add Gamepad") {
      for (const element of deviceList.querySelectorAll(".deviceList li.selected")) {
        element.classList.remove("selected");
      }
      li.classList.add("selected");
    }
    cb();
  };
  return li;
}

/* UI for keyboards.
 */
 
function showKeyboard(container) {
  const deviceForm = container.querySelector(".deviceForm");
  deviceForm.innerHTML = "";
  const wrapper = document.createElement("DIV");
  wrapper.classList.add("keyboard");
  deviceForm.appendChild(wrapper);
  const config = container._gamekInputConfigContent.keyboard || [];
  const table = document.createElement("TABLE");
  wrapper.appendChild(table);
  
  let col = 0, tr = null;
  for (const map of config) {
    if (!tr) {
      tr = document.createElement("TR");
      table.appendChild(tr);
    }
    addKeyboardMap(tr, map);
    switch (col++) {
      case 0: tr.appendChild(document.createElement("TD")); break; // spacer
      case 1: col = 0; tr = null; break; // end of row
    }
  }
  
  tr = document.createElement("TR");
  table.appendChild(tr);
  const td = document.createElement("TD");
  tr.appendChild(td);
  td.setAttribute("colspan", 3);
  const button = document.createElement("BUTTON");
  td.appendChild(button);
  button.innerText = "+";
  button.onclick = newKeyboardMap;
}

function addKeyboardMap(tr, map, td) {
  
  if (!td) {
    td = document.createElement("TD");
    tr.appendChild(td);
  }
  
  if (map) {
    const inputKey = document.createElement("INPUT");
    inputKey.type = "text";
    inputKey.value = map.key;
    inputKey.setAttribute("list", "gamekInputConfigModal-keyboard-keys");
    td.appendChild(inputKey);
    
    const inputPlayer = document.createElement("SELECT");
    addSelectOption(inputPlayer, "1", "1");
    addSelectOption(inputPlayer, "2", "2");
    addSelectOption(inputPlayer, "3", "3");
    addSelectOption(inputPlayer, "4", "4");
    inputPlayer.value = map.player.toString();
    td.appendChild(inputPlayer);
    
    const inputButton = document.createElement("SELECT");
    addSelectOption(inputButton, "LEFT", "LEFT");
    addSelectOption(inputButton, "RIGHT", "RIGHT");
    addSelectOption(inputButton, "UP", "UP");
    addSelectOption(inputButton, "DOWN", "DOWN");
    addSelectOption(inputButton, "SOUTH", "SOUTH");
    addSelectOption(inputButton, "WEST", "WEST");
    addSelectOption(inputButton, "EAST", "EAST");
    addSelectOption(inputButton, "NORTH", "NORTH");
    addSelectOption(inputButton, "L1", "L1");
    addSelectOption(inputButton, "R1", "R1");
    addSelectOption(inputButton, "L2", "L2");
    addSelectOption(inputButton, "R2", "R2");
    addSelectOption(inputButton, "AUX1", "AUX1");
    addSelectOption(inputButton, "AUX2", "AUX2");
    addSelectOption(inputButton, "AUX3", "AUX3");
    addSelectOption(inputButton, "Unassigned", "");
    inputButton.value = map.button;
    td.appendChild(inputButton);
  }
}

function addSelectOption(select, label, value) {
  const option = document.createElement("OPTION");
  select.appendChild(option);
  option.innerText = label;
  option.value = value;
}

function newKeyboardMap(event) {
  event.preventDefault();
  const table = document.querySelector(".keyboard > table");
  if (!table) return;
  const trs = Array.from(table.querySelectorAll("tr"));
  let tr, td;
  if (trs.length >= 2) {
    // The last <tr> should be the one with the Add button. Second-to-last we can maybe fill in the right column.
    // Each content <tr> should have 3 <td>, and the last one might be empty.
    tr = trs[trs.length - 2];
    td = tr.querySelector("td:nth-child(3)");
    console.log({ tr, td });
    if (!td || td.children.length) {
      tr = td = null;
    }
  }
  if (!tr) {
    tr = document.createElement("TR");
    table.insertBefore(tr, table.querySelector("tr:last-of-type"));
  }
  addKeyboardMap(tr, { key: "", player: 1, button: "" }, td);
  if (!td) { // we made a new row for it, so add the placeholders too
    tr.appendChild(document.createElement("TD")); // spacer
    addKeyboardMap(tr, null);
  }
}

function readKeyboardContentFromUi(form) {
  const config = [];
  // Everything we're interested in is a <td> with <input><select><select>
  for (const td of form.querySelectorAll("td")) {
    const keyInput = td.querySelector("input");
    const playerInput = td.querySelector("select:nth-of-type(1)");
    const buttonInput = td.querySelector("select:nth-of-type(2)");
    if (!keyInput || !playerInput || !buttonInput) continue; // This is fine, there are uninteresting tds too.
    const key = keyInput.value;
    const player = +playerInput.value;
    const button = buttonInput.value;
    if (!key) continue; // Reject empty. We could also check is it in keyNames, but that's not necessarily complete.
    if (!button) continue; // "Unassigned" is the empty string, and means we should drop this item.
    config.push({ key, player, button });
  }
  return config;
}

/* UI for touch.
 */
 
function showTouch(container) {
  const deviceForm = container.querySelector(".deviceForm");
  deviceForm.innerHTML = "";
  const wrapper = document.createElement("DIV");
  wrapper.classList.add("touch");
  deviceForm.appendChild(wrapper);
  
  const disclaimer = document.createElement("DIV");
  wrapper.appendChild(disclaimer);
  disclaimer.innerText = "TODO touch input configuration";
}

function readTouchContentFromUi(form) {
  return null;//TODO touch input configuration
}

/* UI for MIDI.
 */
 
function showMidi(container) {
  const deviceForm = container.querySelector(".deviceForm");
  deviceForm.innerHTML = "";
  const wrapper = document.createElement("DIV");
  wrapper.classList.add("midi");
  deviceForm.appendChild(wrapper);
  
  const disclaimer = document.createElement("DIV");
  wrapper.appendChild(disclaimer);
  disclaimer.innerText = "TODO midi configuration";
}

function readMidiContentFromUi(form) {
  return null;//TODO midi configuration
}

/* UI for gamepad.
 */
 
function showGamepad(container, gamepad) {
  if (!gamepad) gamepad = {};
  const deviceForm = container.querySelector(".deviceForm");
  deviceForm.innerHTML = "";
  const wrapper = document.createElement("DIV");
  wrapper.classList.add("gamepad");
  deviceForm.appendChild(wrapper);
  
  const idField = document.createElement("TEXTAREA"); // sic TEXTAREA, gamepad ids can be huge
  wrapper.appendChild(idField);
  idField.name = "id";
  idField.value = gamepad.id || "";
  
  const columnPacker = document.createElement("DIV");
  wrapper.appendChild(columnPacker);
  columnPacker.classList.add("columns");
  
  const axes = gamepad.axes || [];
  const axisColumn = document.createElement("DIV");
  columnPacker.appendChild(axisColumn);
  addGamepadColumnHeader(axisColumn, "Axes");
  for (let i=0; i<axes.length; i++) {
    addGamepadSource(axisColumn, "axis", i, axes[i]);
  }
  addGamepadAdd(axisColumn, "axis");
  
  const buttons = gamepad.buttons || [];
  const buttonColumn = document.createElement("DIV");
  columnPacker.appendChild(buttonColumn);
  addGamepadColumnHeader(buttonColumn, "Buttons");
  for (let i=0; i<buttons.length; i++) {
    addGamepadSource(buttonColumn, "button", i, buttons[i]);
  }
  addGamepadAdd(buttonColumn, "button");
}

function addGamepadColumnHeader(column, label) {
  const header = document.createElement("H3");
  column.appendChild(header);
  header.innerText = label;
}

function addGamepadSource(column, type, index, value) {
  const row = document.createElement("DIV");
  column.appendChild(row);
  const label = document.createElement("LABEL");
  row.appendChild(label);
  label.setAttribute("data-srcbtnid", `${type}:${index}`);
  const span = document.createElement("SPAN");
  label.appendChild(span);
  span.innerText = index;
  
  const inputButton = document.createElement("SELECT");
  if (type === "axis") {
    addSelectOption(inputButton, "HORZ", "HORZ");
    addSelectOption(inputButton, "VERT", "VERT");
  }
  addSelectOption(inputButton, "LEFT", "LEFT");
  addSelectOption(inputButton, "RIGHT", "RIGHT");
  addSelectOption(inputButton, "UP", "UP");
  addSelectOption(inputButton, "DOWN", "DOWN");
  addSelectOption(inputButton, "SOUTH", "SOUTH");
  addSelectOption(inputButton, "WEST", "WEST");
  addSelectOption(inputButton, "EAST", "EAST");
  addSelectOption(inputButton, "NORTH", "NORTH");
  addSelectOption(inputButton, "L1", "L1");
  addSelectOption(inputButton, "R1", "R1");
  addSelectOption(inputButton, "L2", "L2");
  addSelectOption(inputButton, "R2", "R2");
  addSelectOption(inputButton, "AUX1", "AUX1");
  addSelectOption(inputButton, "AUX2", "AUX2");
  addSelectOption(inputButton, "AUX3", "AUX3");
  addSelectOption(inputButton, "Unassigned", "");
  inputButton.name = `${type}:${index}`;
  inputButton.value = value;
  label.appendChild(inputButton);
}

function addGamepadAdd(column, type) {
  const button = document.createElement("INPUT");
  column.appendChild(button);
  button.classList.add("addGamepadSourceButton");
  button.type = "button";
  button.value = "+";
  button.onclick = () => console.log(`TODO: add ${type}`);
}

// Unlike the other read*ContentFromUis, we return just one element of the config's list.
// (same way we received it).
function readGamepadContentFromUi(form) {
  const id = form.querySelector("textarea[name='id']").value.trim();
  if (!id) return null;
  const gamepad = { id, axes: [], buttons: [] };
  
  for (let i=0; ; i++) {
    const input = form.querySelector(`select[name='axis:${i}']`);
    if (!input) break;
    gamepad.axes.push(input.value);
  }
  for (let i=0; ; i++) {
    const input = form.querySelector(`select[name='button:${i}']`);
    if (!input) break;
    gamepad.buttons.push(input.value);
  }
  
  return gamepad;
}

/* Add a blank gamepad config and display it.
 */
 
function addGamepad(container, gamepad) {
  const deviceList = container.querySelector(".deviceList");
  if (!gamepad) {
    gamepad = {
      id: "Unnamed Gamepad",
    };
  }
  container._gamekInputConfigContent.gamepads.push(gamepad);
  const li = addDeviceToList(deviceList, gamepad.id, () => showGamepad(container, gamepad));
  deviceList.insertBefore(li, deviceList.children[deviceList.children.length - 2]);
  li.click();
}

/* Event handling.
 */

function attachInputListener(container, input) {
  return input.listen((d, b, v) => receiveRawInput(container, d, b, v));
}

function receiveRawInput(container, device, button, value) {
  //console.log(`receiveRawInput ${device} ${button} ${value}`);
  
  if (device === "addGamepad") {
    addGamepad(container, button);
    return;
  }
  
  // Does this device match the currently editing gamepad?
  const gamepadIdField = container.querySelector(".gamepad *[name='id']");
  if (gamepadIdField && (gamepadIdField.value === device)) {
    let [type, index] = button.split('-');
    if (type === "axis") ;
    else if (type === "btn") type = "button";
    else type = "";
    if (type && index) {
      const label = container.querySelector(`label[data-srcbtnid='${type}:${index}']`);
      if (label) {
        if (value) label.classList.add("activeButtonHighlight");
        else label.classList.remove("activeButtonHighlight");
        return;
      }
    }
  }
  
  // Is there any value here in keyboard events? I'm thinking probably not.
  // The highlight is meant to address "what is this button's name?", usually not a problem for keyboards.
}

function awaitDismissal(container) {
  container._gamekInputConfigPromise = new Promise((resolve, reject) => {
    container._gamekInputConfigResolve = resolve;
    container._gamekInputConfigReject = reject;
  });
  return container._gamekInputConfigPromise;
}

function submitInputConfigModal(container, event) {
  event.preventDefault();
  const value = container._gamekInputConfigContent;
  container._gamekInputConfigResolve(value);
}

function dismissInputConfigModal(container) {
  container._gamekInputConfigResolve(null);
}

function onInputConfigChange(container, event) {
  let form;
  
  if (form = container.querySelector(".deviceForm > .keyboard")) {
    container._gamekInputConfigContent.keyboard = readKeyboardContentFromUi(form);
    return;
  }
  
  if (form = container.querySelector(".deviceForm > .touch")) {
    container._gamekInputConfigContent.touch = readTouchContentFromUi(form);
    return;
  }
  
  if (form = container.querySelector(".deviceForm > .midi")) {
    container._gamekInputConfigContent.midi = readMidiContentFromUi(form);
    return;
  }
  
  if (form = container.querySelector(".deviceForm > .gamepad")) {
    const gamepad = readGamepadContentFromUi(form);
    container._gamekInputConfigContent.gamepads = combineGamepads(container._gamekInputConfigContent.gamepads, gamepad);
    return;
  }
}

function combineGamepads(list, incoming) {
  if (!incoming) return list;
  if (!list) list = [];
  const p = list.findIndex(g => g.id === incoming.id);
  if (p >= 0) list[p] = incoming;
  else list.push(incoming);
  return list;
}

/* Scraped from https://developer.mozilla.org/en-US/docs/Web/API/UI_Events/Keyboard_event_code_values
 * const codes = new Set();
 * for (const element of document.querySelectorAll("code")) {
 *   const text=element.innerText.trim(); 
 *   if (text.match(/^".*"$/)) codes.add(text.replaceAll('"',''));
 * }
 */
const keyNames = [
  "Escape","Digit1","Digit2","Digit3","Digit4","Digit5","Digit6","Digit7","Digit8","Digit9","Digit0",
  "Minus","Equal","Backspace","Tab","KeyQ","KeyW","KeyE","KeyR","KeyT","KeyY","KeyU","KeyI","KeyO","KeyP",
  "BracketLeft","BracketRight","Enter","ControlLeft","KeyA","KeyS","KeyD","KeyF","KeyG","KeyH","KeyJ","KeyK",
  "KeyL","Semicolon","Quote","Backquote","ShiftLeft","Backslash","KeyZ","KeyX","KeyC","KeyV","KeyB","KeyN","KeyM",
  "Comma","Period","Slash","ShiftRight","NumpadMultiply","AltLeft","Space","CapsLock",
  "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","Pause","ScrollLock","Numpad7","Numpad8","Numpad9","NumpadSubtract",
  "Numpad4","Numpad5","Numpad6","NumpadAdd","Numpad1","Numpad2","Numpad3","Numpad0","NumpadDecimal","PrintScreen",
  "IntlBackslash","F11","F12","NumpadEqual","F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
  "KanaMode","Lang2","Lang1","IntlRo","Lang4","Lang3","Convert","NonConvert","IntlYen","NumpadComma","Undo","Paste",
  "MediaTrackPrevious","Cut","Copy","MediaTrackNext","NumpadEnter","ControlRight","LaunchMail","AudioVolumeMute",
  "LaunchApp2","MediaPlayPause","MediaStop","Eject","VolumeDown","AudioVolumeDown","VolumeUp","AudioVolumeUp","BrowserHome",
  "NumpadDivide","AltRight","Help","NumLock","Home","ArrowUp","PageUp","ArrowLeft","ArrowRight","End","ArrowDown","PageDown",
  "Insert","Delete","OSLeft","MetaLeft","OSRight","MetaRight","ContextMenu","Power","Sleep","WakeUp","BrowserSearch",
  "BrowserFavorites","BrowserRefresh","BrowserStop","BrowserForward","BrowserBack","LaunchApp1","MediaSelect","Fn","VolumeMute",
  "Lang5","Abort","Again","Props","Select","Open","Find","NumpadParenLeft","NumpadParenRight","BrightnessUp",
];
