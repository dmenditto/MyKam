# Testing MyKam with the numpad

An example key layout that puts the slots on the numpad. It is only a suggestion: bind the commands to any
free keys. Turn **Num Lock ON** before testing: with Num Lock off, the numpad keys send arrows and X-Plane sees different keys.

---

## 1. Key bindings

### 1.1 Suggested layout

| Key | Command | Note |
|-----|---------|------|
| Numpad 1 ... Numpad 9 | `mykam/go_slot_1` ... `mykam/go_slot_9` | replaces X-Plane's `sim/view/quick_look_N` on these keys |
| Ctrl + Alt + Numpad 1 ... 9 | `mykam/save_slot_1` ... `mykam/save_slot_9` | free in the default setup |
| Numpad 0 | `mykam/go_slot_0` | replaces `sim/view/quick_look_0` |
| Ctrl + Alt + Numpad 0 | `mykam/save_slot_0` | free in the default setup |
| Numpad . | `mykam/toggle_active` | turns MyKam off and on |

Optional: remove the Ctrl + Numpad 0 ... 9 bindings (`sim/view/quick_look_N_mem`), so a Ctrl press without Alt does not store an X-Plane quick look by mistake.

The slot cards in the MyKam window show these hints from `key_hint_go` and `key_hint_save` in `mykam.ini`. They are only labels.

### 1.2 How to bind (in X-Plane)
1. Open Settings (the gear icon at the top) and go to **Keyboard**.
2. In the search box type the command name, for example `mykam/go_slot_5`.
3. Click the binding box on the right of the command, press the key (Numpad 5), confirm. If X-Plane says the key is used by `quick_look_5`, accept to replace it.
4. Repeat for every row of 1.1.
5. Search `mykam` and check you see: go 0-9 on Numpad 0-9, save 0-9 on Ctrl + Alt + Numpad 0-9, on/off on Numpad . (release can stay unbound: the quick bar's Release button and Numpad . both give the camera back).
6. Close Settings.

---

## 2. Keys used in the tests (X-Plane defaults; adjust to yours)

| Action | Key |
|--------|-----|
| Free camera | **C** |
| Default forward view | **W** |
| 3D cockpit | **Shift + 0** |
| Linear spot / Still spot / Runway / Circle / Tower / Ride-along / Chase / Wingman | **Shift + 1 / 2 / 3 / 4 / 5 / 6 / 8 / 9** |
| Move left / right / up / down | **Left / Right / Up / Down** arrows |
| Move forward / back | **.** / **,** |
| Pan left / right, tilt up / down | **Q / E**, **R / F** |
| Zoom in / out | **=** / **-** |
| Faster / slower | add **Shift** / **Ctrl** |
| Mouse look | hold **right mouse button** and move |
| Frame rate | **Shift + Ctrl + F** |
| Log | `<X-Plane 12>\Log.txt`, search `[MyKam]` |

---

## 3. Tests

Start X-Plane 12 with the Cessna 172 on a runway, other camera plugins disabled (Plugins > Plugin Admin).
Write Pass / Fail and what you saw in each "Result" line.

### T1 Cockpit slot
1. **Shift + 0** (3D cockpit). Right-drag to look at the left window, move the head a little with the arrows.
2. **Ctrl + Alt + Numpad 1** (save slot 1). Log: `saved camera to slot 1 (cockpit view)`.
3. Right-drag to look at the panel.
4. **Numpad 1**.
- Pass: the head glides back to the left window view in about 1.5 s, and afterwards mouse look and all keys work normally.

Result:

### T2 Free camera slot
1. **C**. Place the camera beside the left wing with the mouse and keys.
2. **Ctrl + Alt + Numpad 2**.
3. **W**, then **Numpad 2**.
- Pass: glide to the spot, then you are in X-Plane's free camera there (mouse and keys exactly like after pressing C). Log: `handed over to the X-Plane sim/view/free_camera view at slot 2`.

Result:

### T3 Saved view types (one slot each)
For each row: press the view key, adjust it with mouse and keys, press the save key, press **W**, press the go key, wait for the glide, then try mouse and keys.

| View | View key | Save | Go | Stayed or jumped? |
|------|----------|------|----|-------------------|
| Chase | Shift + 8 | Ctrl + Alt + Numpad 3 | Numpad 3 | |
| Circle | Shift + 4 | Ctrl + Alt + Numpad 4 | Numpad 4 | |
| Tower | Shift + 5 | Ctrl + Alt + Numpad 5 | Numpad 5 | |
| Linear spot | Shift + 1 | Ctrl + Alt + Numpad 6 | Numpad 6 | |
| Still spot | Shift + 2 | Ctrl + Alt + Numpad 7 | Numpad 7 | |
| Runway | Shift + 3 | Ctrl + Alt + Numpad 8 | Numpad 8 | |
| Ride-along | Shift + 6 | Ctrl + Alt + Numpad 9 | Numpad 9 | |
| Wingman | Shift + 9 | reuse slot 3: Ctrl + Alt + Numpad 3 | Numpad 3 | |

- Stayed (pass): X-Plane ends in that view type at your saved spot, mouse and keys behave as that view normally does. Log: `handed over to the X-Plane sim/view/<view> view at slot N`.
- Jumped: when the glide ends the view moves elsewhere. Note which views jump; those are good candidates for `handoff_view = free`.

### T4 Switching between slots
1. **Numpad 1** (cockpit), then **Numpad 2** (free camera), then **Numpad 4**, and press **Numpad 2** again before the glide ends.
- Pass: every switch is smooth, the last one changes direction without a jump.

Result:

### T5 Release
1. **Numpad 2**, then while it glides click **Release** in the quick bar (or press a key bound to `mykam/release`).
- Pass: normal X-Plane control comes back straight away.

Result:

### T6 Empty slot
1. If a slot is still empty (check the log after saving), press its go key.
- Pass: nothing happens, log shows `slot N is empty`.

Result:

### T7 Banked flight
1. Take off, climb, then **Numpad 2** and **Numpad 1** while banking 30 degrees.
- Pass: the cameras stay in place relative to the aircraft and the horizon looks right.

Result:

### T8 Restart
1. Quit X-Plane, start it again with the same aircraft, press **Numpad 1**, **Numpad 2**, **Numpad 4**.
- Pass: all slots still work. File: `mykam_views.txt` in the aircraft's own folder.

Result:

### T9 Frame rate
1. **Shift + Ctrl + F**. Note FPS in a normal view, then while switching between **Numpad 2** and **Numpad 4** for 30 s.
- Pass: no visible drop.

Result:

---

If a view type jumps, or you prefer every outside slot to end in the free camera, add `handoff_view = free` to `Resources\plugins\MyKam\mykam.ini` and restart X-Plane.
