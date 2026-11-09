# Testing MyKam in X-Plane 12

A manual test script for the in-sim behaviour that the unit tests cannot cover. Write the result of each
test (Pass / Fail plus what you saw) in its "Result" line. The key suggestions below are only suggestions:
use any free keys in your own X-Plane keyboard settings.

---

## 0. Setup (once)

### 0.1 Install the plugin
1. Quit X-Plane 12 completely.
2. Unzip the release zip (or take `build\MyKam` from a Release build) and put the `MyKam` folder into
   `<X-Plane 12>\Resources\plugins\`. If a `MyKam` folder is already there, replace the files.
3. Check the result is exactly:
   - `...\Resources\plugins\MyKam\win_x64\MyKam.xpl`
   - `...\Resources\plugins\MyKam\mykam.ini`
   - `...\Resources\plugins\MyKam\fonts\` (with the font files)

### 0.2 Other camera plugins
1. Start X-Plane 12 and load a flight with a simple aircraft (for example the default Cessna 172) on a runway.
2. Top menu **Plugins > Plugin Admin**.
3. If you use another camera plugin, untick **Enabled** next to it while testing.
4. In the same list check **MyKam** is present and ticked.

### 0.3 Bind the MyKam commands
1. Open Settings (the gear icon at the top) and go to the **Keyboard** tab.
2. In the search box type `mykam`. You should see 26 commands.
3. For each command below, click the box on its right, press the key combination, and confirm:

| Command | Suggested key |
|---------|---------------|
| `mykam/save_slot_1` ... `mykam/save_slot_9` | **Shift + Alt + 1** ... **Shift + Alt + 9** |
| `mykam/go_slot_1` ... `mykam/go_slot_9` | **Alt + 1** ... **Alt + 9** |
| `mykam/release` | **Alt + 0** |

The other commands are `mykam/toggle_window`, `mykam/toggle_active`, `mykam/next`, `mykam/previous` and `mykam/toggle_group`.

4. Close Settings.

### 0.4 X-Plane keys used in the tests (default bindings; adjust to yours)

| Action | Key | X-Plane command |
|--------|-----|-----------------|
| 3D cockpit view | **Shift + 0** | `sim/view/3d_cockpit_cmnd_look` |
| Default forward view | **W** | `sim/view/default_view` |
| Chase view (outside) | **Shift + 8** | `sim/view/chase` |
| Circle / orbit view (outside) | **Shift + 4** | `sim/view/circle` |
| Free camera (outside) | **C** | `sim/view/free_camera` |
| Move view left / right | **Left / Right arrow** | `sim/general/left`, `right` |
| Move view up / down | **Up / Down arrow** | `sim/general/up`, `down` |
| Move view forward / back | **.** / **,** | `sim/general/forward`, `backward` |
| Pan left / right | **Q / E** | `sim/general/rot_left`, `rot_right` |
| Tilt up / down | **R / F** | `sim/general/rot_up`, `rot_down` |
| Zoom in / out | **=** / **-** | `sim/general/zoom_in`, `zoom_out` |
| Faster / slower versions | add **Shift** / **Ctrl** | `..._fast`, `..._slow` |
| Show frame rate | **Shift + Ctrl + F** | `sim/operation/show_fps` |
| Mouse look in cockpit | hold **right mouse button** and move | built in |

X-Plane also has its own quick views (`sim/view/quick_look_N`). They are X-Plane's, not MyKam's; do not mix them up during the tests.

### 0.5 Where to read the log
`<X-Plane 12>\Log.txt`. Open it after a test (X-Plane keeps writing to it; reopen to refresh). Search for `[MyKam]`.

---

## IT-01 Plugin loads
1. After starting X-Plane, open `Log.txt` and search `[MyKam]`.

Pass when: you see `[MyKam] started, version 0.9.0-beta` and a line like `[MyKam] aircraft 'Cessna_172SP.acf': 0 saved camera(s) loaded`. No line containing `error:`.

Result:

## IT-02 Commands exist
1. Settings > **Keyboard**, search `mykam`.

Pass when: 26 commands are listed (save 0-9, go 0-9, release, toggle_window, toggle_active, next, previous, toggle_group), all starting with `mykam/`.

Result:

## IT-03 Cockpit camera: save and go back
1. Press **Shift + 0** (3D cockpit).
2. Hold the **right mouse button** and move the mouse to look at the left window. Use **Up arrow** / **,** to move your head a bit.
3. Press **Shift + Alt + 1** (save slot 1). Log should show `[MyKam] saved camera to slot 1`.
4. Look somewhere else with the mouse (for example at the panel).
5. Press **Alt + 1** (go to slot 1).

Pass when: the head glides in about 1.5 s back to the left-window view.

Result:

## IT-04 Cockpit camera: you keep control afterwards
1. Right after IT-03, hold the **right mouse button** and move the mouse.
2. Press **Q / E**, **R / F**, the arrows, **.** and **,**, and **= / -**.

Pass when: the mouse and all these keys move the view normally, like before you used MyKam. Also note if any direction moves the wrong way during the glide in IT-03.

Result:

## IT-05 Cockpit camera from an outside view
1. Press **Shift + 8** (chase view, outside).
2. Press **Alt + 1**.

Pass when: X-Plane switches to the 3D cockpit and the head glides to the slot 1 view (and stays there, it does not snap back to the default head position).

Result:

## IT-06 Outside camera: save and go back
1. Press **Shift + 4** (circle view) or **C** (free camera). Move the camera beside the left wing with the arrows, **.** / **,** and **Q / E**.
2. Press **Shift + Alt + 2** (save slot 2).
3. Press **W** (forward view).
4. Press **Alt + 2**.

Pass when: the camera glides to the saved spot beside the wing.

Result:

## IT-07 Outside camera: keys move the held camera
1. While on slot 2 (after IT-06), press and hold each of: **Left/Right**, **Up/Down**, **. / ,**, **Q / E**, **R / F**, **= / -**. Then try the same with **Shift** (fast) and **Ctrl** (slow).
2. Hold the **right mouse button** and drag to orbit around the aircraft (left/right turns round it, up/down raises and lowers the camera). Use the **mouse wheel** to move closer or farther. A normal left click on an X-Plane menu or window must still work.
3. Press **Shift + Alt + 3** to save this adjusted view to slot 3, then **Alt + 2**, then **Alt + 3**.

Pass when: each key moves the camera in the direction its name says (left goes left, forward goes towards where the camera looks, Q pans left...), Shift is faster and Ctrl slower, and slot 3 returns to the adjusted view.

Pass when (mouse): right-drag orbits the camera round the aircraft and the aircraft stays in the same place on screen (like the chase view), the camera stops at about 85 degrees above and below, the wheel moves it closer or farther in steps (not nearer than 2 m, not farther than 2000 m), left clicks still reach X-Plane menus and windows, and the saved slot 3 returns to the orbited and zoomed view. In a cockpit slot the mouse is X-Plane's own again.

Result:

## IT-08 Saved camera follows the aircraft when banking (sign check)
1. Take off, climb to a safe altitude.
2. Press **Alt + 2** (outside slot beside the wing).
3. Roll into a 30 degree bank left, then right.

Pass when: the camera stays beside the wing and rolls with the aircraft (the wing stays in the same place on screen; the horizon tilts). Fail if the aircraft rolls one way and the camera the other, or the camera drifts away.

Result:

## IT-09 Transition between two slots
1. In flight, press **Alt + 1** (cockpit) then **Alt + 2** (outside), then **Alt + 3**, then press **Alt + 2** again before the move finishes.

Pass when: each switch is smooth (about 1.5 s), heading turns the short way, and the last press changes direction smoothly from where the camera was, with no jump.

Result:

## IT-10 Release
1. On an outside slot, press **Alt + 0** (`mykam/release`).
2. Go to the slot again with **Alt + 2**, then press **Shift + 8** (any X-Plane view key).

Pass when: both give normal X-Plane control back (mouse and keys work as usual).

Result:

## IT-11 Empty slot
1. Press **Alt + 9** (nothing saved there).

Pass when: nothing changes on screen and `Log.txt` shows `[MyKam] slot 9 is empty`.

Result:

## IT-12 Saved cameras survive a restart
1. Quit X-Plane, start it again with the same aircraft.
2. Press **Alt + 1**, **Alt + 2**, **Alt + 3**.

Pass when: all three work, and the file `mykam_views.txt` exists in the aircraft's own folder (next to its `.acf` file).

Result:

## IT-13 Aircraft change
1. While on slot 2, use the Flight Configuration to load a different aircraft.

Pass when: the view returns to normal X-Plane control, the log shows the new aircraft with its own count (0 the first time), and **Alt + 1** on the new aircraft says the slot is empty.

Result:

## IT-14 Hand-off to the X-Plane free camera
1. Go to an outside slot with **Alt + 2** and wait for the move to finish.
2. Look around with the mouse and the keys, and compare with pressing **C** yourself.

Pass when: X-Plane is in its own free camera at the saved spot, the mouse and keys behave exactly like pressing **C**, and `Log.txt` shows `[MyKam] handed over to the X-Plane free camera at slot 2`. Fail if the view jumps to another place (then X-Plane does not start its free camera from the current view).

To get the old behaviour back (MyKam keeps the camera), set `handoff_view = off` in `Resources\plugins\MyKam\mykam.ini` (the older `free_camera_handoff = 0` still works) and restart X-Plane or reload the plugin. Note: a slot saved right after pressing **C** stores the free camera, so IT-14 still ends in the free camera; slots saved in another outside view are covered by IT-15.

Result:

## IT-15 Hand-off to the saved view type
MyKam remembers the last view command you ran (for example `sim/view/chase`), stores it with an outside slot, and after the glide hands the camera back to that same X-Plane view type. Without a stored view it uses the free camera.

Do this once for each view: **Chase (Shift + 8)**, **Circle (Shift + 4)**, **Tower (Shift + 5)**, **Linear spot (Shift + 1)**, **Still spot (Shift + 2)**, **Runway (Shift + 3)**, **Ride-along (Shift + 6)**, **Wingman (Shift + 9)**.
1. Press the view key and adjust the view (mouse and keys as X-Plane allows) so it looks different from where a go-to would start.
2. Save it to a slot with **Shift + Alt + N** (use a different slot per view).
3. Press **W** (default forward view) so the go-to starts from the cockpit.
4. Go to the slot with **Alt + N** and wait for the glide to finish.
5. Check the view type (same look and behaviour as when you pressed the key) and try the standard mouse and keys for that view.

Pass when, for each view: X-Plane ends in that view type at the saved spot, the standard mouse and keys work, and `Log.txt` shows `[MyKam] handed over to the X-Plane sim/view/<name> view at slot N` (for example `sim/view/chase`). Also open the aircraft's `mykam_views.txt` and confirm the view command is stored with the view.

Per view, write down whether it stayed at the saved spot or jumped (some views, such as tower or runway, may place their own camera on entry and move away from the saved spot):

| View | Stayed at the saved spot | Jumped |
|------|--------------------------|--------|
| Chase | | |
| Circle | | |
| Tower | | |
| Linear spot | | |
| Still spot | | |
| Runway | | |
| Ride-along | | |
| Wingman | | |

Fallback: if a view jumps or behaves badly, set `handoff_view = free` in `Resources\plugins\MyKam\mykam.ini` (restart X-Plane or reload the plugin). Every outside slot then hands over to the free camera, which IT-14 showed starts from the current view. `handoff_view = saved` is the default and `off` keeps MyKam holding the camera.

Result:

---

## Performance (NFR1.1, NFR1.2)
1. Press **Shift + Ctrl + F** to show the frame rate, park somewhere fixed, note the FPS with MyKam idle (normal view).
2. Hold a slot (**Alt + 2**) for 30 s, note FPS.
3. Switch between **Alt + 2** and **Alt + 3** repeatedly for 30 s, note FPS.

Pass when: no visible drop. As a number: frame time is 1000 / FPS ms, the difference to idle should be under 0.2 ms while holding and under 1 ms while switching.

Result:

## Soak test (NFR2.1)
Fly normally for 30 minutes and use saves and switches often (aim for 50 in total).

Pass when: no crash or freeze, and `Log.txt` has no `[MyKam] error:` line (lines with `warning:` are fine).

Result:

---

When you report a problem, include the failing Result lines and the `[MyKam]` lines from `Log.txt`.
