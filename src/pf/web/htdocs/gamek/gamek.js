/* gamek.js
 * Public interface to the Gamek runtime.
 */
 
import { GamekController } from "./GamekController.js";
/* Important public bits of GamekController:
 *   enableLogging: boolean // if true (default), fprintf in the game prints as console.log here.
 *
 *   begin(): void // A new controller is already begun.
 *   end(): void // Hard pause. Safe to 'begin' again after.
 *   // We don't react to visibilitychange events, but you should.
 *
 *   files:
 *     readLocalStorage: boolean
 *     writeLocalStorage: boolean
 *   input:
 *     getConfiguration(): Config
 *     setConfiguration(Config): void
 *     onKillTouchEvents: () => {} ; Replace this to get notified when we disable touch due to dpad triple tap.
 *     listen(cb): listenerid // cb(deviceName, buttonName, value), raw pre-map events
 *     Config:
 *       keyboard: { key:string, player:integer, button:string }[]
 *       gamepads: { id:string, axes:string[], buttons:string[] }[]
 *         (axes) and (buttons) are indexed the same as the device exposed by Gamepad API.
 *         Values are empty, a button name, or for axes "HORZ" or "VERT".
 *       touch: TODO
 *       midi: TODO
 */

/* Create a new Gamek runtime using the Wasm file (bin), and attach it to <canvas> element (canvas).
 * If another runtime was previously attached to this element, we drop it gracefully.
 * Returns a Promise that resolves to the new GamekController, once it's running.
 * (audioRate) can be false for the default, currently 44100.
 */
export function gamek_init(canvas, bin, audioRate) {
  const controller = new GamekController(audioRate);
  return controller.loadBinary(bin).then(() => {
    controller.installInDom(canvas);
    controller.begin();
    return controller;
  });
}
