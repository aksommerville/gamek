/* gamek.js
 * Public interface to the Gamek runtime.
 */
 
import { GamekController } from "./GamekController.js";
/* Important public bits of GamekController:
 *   enableLogging: boolean // if true (default), fprintf in the game prints as console.log here.
 *   begin(): void // A new controller is already begun.
 *   end(): void // Hard pause. Safe to 'begin' again after.
 *   files:
 *     readLocalStorage: boolean
 *     writeLocalStorage: boolean
 *   input:
 *     keyMap: { [KeyCode]: [playerid, btnid] }
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
