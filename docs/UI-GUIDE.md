# MyKam window and quick bar (UI guide)

MyKam has a window for managing your saved views (up to 99 per aircraft, in two groups: **Internal** for cockpit views and **External** for free camera and chase views) and a slim quick bar for fast switching. Both are drawn inside X-Plane with Dear ImGui 1.91.5. The commands and key bindings from `TESTING-NUMPAD.md` work as before; the window is an extra. A view does not need a key.

## Opening the window

- Menu: **Plugins > MyKam > Show window** (and **Hide window**).
- Command `mykam/toggle_window` (bind it to a key, see below).
- The quick bar has a **Menu** button that toggles the window as well.
- The window is hidden every time X-Plane starts. It draws its own frame (dark glass panel, rounded corners, thin cyan border, soft shadow) and has no X-Plane title bar. Drag the **header** to move it, drag the **edges** to resize it. The header icons on the right are **pop-out** (the window becomes its own operating system window; click again to bring it back), **minimise** (collapses to the header; click again to restore the size) and **close** (same as Hide window).

## Look and fonts

Dark translucent glass, cyan accent, rounded shapes. Text is Inter (Regular and SemiBold) and icons are Material Icons. The font files are loaded at run time from the `fonts` folder next to `win_x64` in the plugin folder (`deploy.ps1` copies it). If a font file is missing or damaged, the window uses the small built-in font (and plain letters instead of icons) and `Log.txt` gets one `[MyKam] warning:` line; nothing else changes. On a high-DPI X-Plane (two screen pixels per boxel) the fonts are built at twice the size so text stays sharp.

## Header

Camera icon, `MyKam`, and a status pill: a green dot with `X-Plane in control`, or a cyan dot with the name and type of the active view, for example `Left wing · Free camera` or `Cockpit view 1 · Cockpit`. The pill goes back to `X-Plane in control` when you release, choose an X-Plane view yourself, load another aircraft, or another plugin takes the camera.

## Sidebar

On the left (about 180 px): **Internal** and **External**, each with an icon and a count badge (the number of views in the group), then a divider, then **X-Plane views** and **Settings**. The selected page has a cyan bar on its left and a lighter background. The plugin version sits at the bottom. The right side shows the page title with a one-line muted subtitle under it. There is no separate Views page any more.

## Internal and External pages

The two pages work the same way. **Internal** lists the cockpit views, **External** lists the free camera and chase views.

At the top is the pill button **+ Save current view**. It saves the view X-Plane shows now as a new view of this group, with the default name `Cockpit view N` or `Outside view N` (the lowest free number) and no key. It is enabled only when the current view fits the group:

- Internal: X-Plane is in a cockpit view.
- External: X-Plane is in the free camera or the chase view, or MyKam holds an outside camera.
- Otherwise (the other group's view, or circle, tower, runway, spot, ride-along, wingman) the button is dimmed. Hover it for a tooltip that says which view to switch to. Nothing is saved.

Below it is a scrolling list, one row per view, **in the order you set** (new views go to the end; a star does not move a view):

- the type icon, the **label badge** (a small cyan circle with the 1 or 2 character label, only when the view has one), the name, a small type chip (**Cockpit**, **Free camera** or **Chase**);
- an amber star: **star = shown in the quick bar** (click to show or hide the view in the bar);
- the key text: the custom key (for example `Ctrl+Shift+W`), or `Num 3` for numpad slot 3, or a muted dash when the view has no key;
- a cyan **Go** pill on the right.

Click anywhere on a row (or **Go**) to go to the view. The active view has a 2 px cyan border and a faint glow; the row lights up under the pointer. An empty group shows a short friendly text and the save button. **Drag a row onto another row of the same group** to put it in that place (a cyan outline marks the row it will take the place of); the new order is saved at once.

Right-click a row for the menu:

- **Edit...**: opens the edit dialog (see below).
- **Update to current view**: shown only when the current X-Plane view fits the row's group. The pose changes; name, label, key, star and place stay.
- **Show in quick bar**: a check mark when the view is starred; click to toggle.
- **Move up** / **Move down**: one place within the group (dimmed at the ends).
- **Delete...**: asks `Delete <name>?` with Yes and No. Yes removes the view from memory and from the file.

## Edit dialog

**Edit...** (from a row menu or from a quick bar chip menu) opens a glass dialog in the middle of the main window, with the rest of the window dimmed. It always opens in the main window, because X-Plane only delivers mouse clicks inside the window they happen in; if you pick **Edit...** on a chip while the main window is hidden or collapsed, the window is shown (and expanded) first. Fields:

- **Name**: up to 32 printable ASCII characters. Empty is not allowed.
- **Quick bar label**: 1 or 2 letters (A to Z, typed in lowercase they turn uppercase) or digits, or nothing. Next to the field a live preview shows the chip as it will look in the quick bar: the label in a round badge, then the name cut to 14 characters. With no label the chip shows the type icon.
- **Key**: *None*, *Numpad slot* or *Custom key*.
  - *Numpad slot* shows a 3 by 3 grid like the numeric keypad (7 8 9 / 4 5 6 / 1 2 3). A slot another view already has shows that view's name; picking it moves the slot here. These are the keys that `mykam/go_slot_N` and `mykam/save_slot_N` use.
  - *Custom key* has a **Press a key** field: click it, then press the key (with Shift, Ctrl or Alt if you want). The field shows the result, for example `Ctrl+Shift+W`. **Esc** cancels the capture, **Backspace** clears the key, and the **Clear** button clears it too. Escape, Backspace and the modifier keys alone cannot be used as a key. Below the field a muted note says: *X-Plane does not tell plugins which keys it uses; if this key is bound in X-Plane, the X-Plane binding may stop working while MyKam uses it.* A key another view already has is shown in amber (`Used by ... Saving moves the key here.`); the combination is unique per aircraft.
  - A view has one kind of key in the dialog: choosing *Custom key* and saving gives up the numpad slot, and the other way round.
- **Show in quick bar**: the star, as a switch.
- **XPRealistic** (only while XPRealistic is loaded, or when the view already has a choice): *Leave as is* (default), *On* or *Off*. Off pauses XPRealistic's effects (its freeze mode) when you go to the view; On resumes them, for a cockpit view once the glide has ended. Stored as `viewN.xprealistic=on|off`. If XPRealistic's master switch is off, MyKam leaves it alone and logs one line. A view with a choice shows an `XPR on` (green) or `XPR off` (amber) chip in its row. While XPRealistic is loaded the quick bar has an XPRealistic button just before Release: filled while its effects run, soft while they are paused; a click pauses or resumes them. It is dimmed while XPRealistic's master switch is off.

A camera plugin running next to MyKam (X-Camera) shows an amber line above every page of the window, with the reason in its tooltip.
- **Cancel** and **Save**. A problem in a field is written in red under it and **Save** stays dimmed until every field is fine. Nothing is stored until **Save**; then everything is written in one go.

The custom key works like a numpad key: pressing it goes to the view. MyKam registers one X-Plane hot key per view that has a custom key (description `MyKam: <view name>`), registers them again when you load another aircraft, edit or delete a view, and removes them when the plugin is disabled or X-Plane quits. X-Plane's hot key list shows them under that description.

Footer: round **Previous** and **Next** buttons step through all views in group order (Internal first, then External, each as listed, wrapping round; with no active view Next goes to the first and Previous to the last). **Release** (outline pill) gives the camera back to X-Plane. The **Transition** slider (cyan track, round knob, value on the right) runs from 0 to 10 s in 0.1 s steps; the value is saved to `mykam.ini` when you let go of the slider. In a very narrow window the slider's label, then the slider itself, are left out; the Settings page always has it.

At most 99 views per aircraft. At the limit a save does nothing except one `Log.txt` line.

## X-Plane views page

A grid of ten large tiles with an icon and a label: 3D cockpit, Free camera, Chase, Circle, Tower, Runway, Linear spot, Still spot, Ride-along, Wingman. Each runs the matching `sim/view/*` command and first releases any camera MyKam holds. The tile of the X-Plane view you chose last is highlighted (cyan border), as long as X-Plane is in control. The note below the grid repeats that saving is available in the cockpit, free camera and chase views.

## Settings page

Grouped panels, each row with a title, a one-line description and its control on the right.

- **Camera motion**
  - **Transition time**: same value as the footer slider.
  - **After the glide**: segmented control *Saved view* (hand over to the X-Plane view the view was saved in), *Free camera* (always the free camera), *Hold* (MyKam keeps the camera). This is `handoff_view = saved | free | off`.
- **Quick bar**
  - **Show quick bar**: a toggle switch (cyan when on), `show_quick_bar`, default on.
  - **Quick bar position**: segmented control *Bottom* or *Top* (`quick_bar_position`).
- **Quick bar buttons**: one row per X-Plane view type (3D cockpit, Free camera, Chase, Circle, Tower, Runway, Linear spot, Still spot, Ride-along, Wingman) with its icon, its name and a toggle switch (same switch as *Show quick bar*). The views that are on come first, in the order the quick bar shows them, each with a small up and a down arrow to move it one place earlier or later (the first up arrow and the last down arrow are dimmed); the views that are off follow in the default order. Turning a view on puts it at the end of the bar. A change shows in the quick bar at once and is written to `mykam.ini` as `quick_bar_views`, a comma separated list of short names (`cockpit`, `free`, `chase`, `circle`, `tower`, `runway`, `linear_spot`, `still_spot`, `ridealong`, `wingman`). Default `cockpit,free,chase`. A name that is not in the list is ignored (one `[MyKam] warning:` line in `Log.txt`), repeats are dropped, any letter case is accepted, and an empty value shows no X-Plane view buttons. Release and Menu are not part of the list.

Every change is written to `mykam.ini` at once, through a temporary file and a rename. Comments, unknown lines and keys such as `key_hint_go` stay where they were; a setting that was not in the file yet is added at the end. If the file cannot be written the setting still works until X-Plane restarts and the page says so in red. A file that cannot be read back is never overwritten.

## Quick bar

A floating glass pill near the bottom centre (or top centre) of the main screen, shown when `show_quick_bar = 1`. It fades to 60 percent when the mouse is not over it and back to full when you hover it. From left to right:

1. **Internal | External switch.** A two-segment pill, each segment an icon and a short label. The selected segment is solid cyan with dark text. Click a segment to show that group.
2. **View chips.** Only the **starred** views of the shown group, in the same order as the window list. A chip has the label in a round badge when the view has a label (otherwise the type icon), then the name (cut to 14 characters with `...`). The active view's chip is solid cyan. Hover shows a tooltip with the full name, the type and the key text (`Ctrl+Shift+W`, `Num 3` or `none`). **Click** goes to the view. **Drag a chip onto another chip** to move it to that place; the order is the group order, so the list in the window changes too. **Right-click** opens a small glass popup: **Update to current view** (first, only when the current X-Plane view fits the group), **Edit...** (opens the edit dialog in the main window), **Move left**, **Move right** (past the next starred view; dimmed at the ends), **Remove from quick bar** (clears the star, the view stays in its list) and **Delete...** (then **Yes** or **No**).
3. **+ button.** Saves the current X-Plane view as a new view in the shown group **and stars it**, so it appears in the bar at once (at the end). It is dimmed when the current view does not fit the group (a cockpit view for Internal; the free camera, the chase view or a camera MyKam holds for External); hovering it then says which view to switch to, the same text as the window's save button.
4. A thin separator, then the **X-Plane view buttons** you chose in Settings > Quick bar buttons (default **Cockpit**, **Free** and **Chase**), in the order of that list, then **Release** (gives the camera back to X-Plane) and **Menu** (shows or hides the main window). Release and Menu always stay, even when no X-Plane view button is chosen. Each view button has its own icon and a tooltip with the full view name (`X-Plane view: Chase`). The button of the X-Plane view you are in now is solid cyan, as long as X-Plane is in control (no saved view is active); the other buttons are dark with a thin outline. The icons (the same as on the X-Plane views page): Cockpit `dashboard`, Free camera `photo_camera`, Chase `flight`, Circle `360`, Tower `cell_tower`, Runway `flight_land`, Linear spot `moving`, Still spot `center_focus_strong`, Ride-along `airline_seat_recline_normal`, Wingman `group`.

**Spacing.** Every round element (label chips, icon buttons) is a true circle drawn 1 px inside its 30 px square, so circles never touch. Chips are 6 px apart, round buttons 6 px apart, and the bar's sections (switch, chips, +, separator, view buttons) 10 px apart. The bar's width and the drawing come from the same layout (`src/core/bar_layout.h`), so the bar is always exactly as wide as what is drawn in it, and its X-Plane window is that wide (rounded up). If the window is narrower than the bar for a moment (a button was just added) the bar waits for the new size instead of being cut off.

**Which group is shown.** The bar follows what you are looking at: **Internal** while X-Plane is in a cockpit view or MyKam is moving the head to a cockpit view, **External** while X-Plane is in an outside view (free camera, chase, circle, tower and so on) or MyKam holds or is handing over an outside camera. Clicking a segment (or running `mykam/toggle_group`) overrides this and the choice stays until the kind of view changes (cockpit to outside or the other way); then the bar follows again.

**Size and many views.** The bar is as wide as its content. The chip area is at most about 60 percent of the main screen width; when the chips do not fit, turn the mouse wheel over the bar to scroll them, or use the small left and right arrows at the ends of the chip area. The active chip is scrolled into view when it changes. With no starred view in the group the bar shows a muted `Star a view to show it here` next to the + button. The bar changes the size of its X-Plane window when its width actually changes (and places it again, at most three times, if the window turns out not to have the wanted size).

**Popup room.** While the right-click popup is open the bar's window is taller (the popup sits above the bar at the bottom of the screen, below it at the top) and a little wider if needed, because X-Plane only delivers clicks inside a window's rectangle. The popup closes when the mouse leaves that area. Clicks outside the bar's own rectangle reach X-Plane as usual, except in that extra area while the popup is open.

The old row of round buttons 1 to 9 is gone from the bar. The numpad slots still work as commands (`mykam/go_slot_N`, `mykam/save_slot_N`); a view's key shows in the window list and in the chip's tooltip, not on the chip.

## Saved views file and migration

Views are kept per aircraft in `mykam_views.txt` in the aircraft's own folder, next to its `.acf`. When that file does not exist yet, the older per-plugin file `cameras\<aircraft>.slots.txt` is read once. `Log.txt` has one line saying so, and the next save writes `mykam_views.txt`. The older file is only read: it is never changed or deleted. It is written through a temporary file and a rename, read with a 64 KB size limit, and every value is checked on load: a view with a bad value is skipped and the others still load. Format 2:

```
format=2
next_id=3
view1.x=1.25
view1.y=0.6
view1.z=-0.4
view1.pitch=-4
view1.heading=10
view1.roll=0
view1.zoom=1
view1.kind=cockpit
view1.name=Left seat
view1.hotkey=3
view2.x=-18
view2.y=6
view2.z=12
view2.pitch=-8
view2.heading=140
view2.roll=0
view2.zoom=1
view2.kind=external
view2.name=Behind the tail
view2.order=1
view2.view=sim/view/chase
view2.favourite=1
view2.label=BT
view2.key=vk:87,flags:shift+ctrl
```

(The lines of view 1 would also carry `view1.order=1`.) `viewID.view`, `viewID.favourite`, `viewID.hotkey`, `viewID.label` and `viewID.key` are optional; `viewID.order` is written for every view. The id is a whole number that is never reused in a file (`next_id` remembers the next one even after deletes). A hotkey is 0 to 9 (numpad 0 is stored as `hotkey=0`) and belongs to one view only.

- `label`: 1 or 2 characters, letters A to Z (stored uppercase) or digits. A bad label is ignored and the view still loads.
- `key`: `vk:<virtual key code>,flags:<modifiers>`. The code is X-Plane's virtual key code (87 is W, see `XPLMDefs.h`). The modifiers are `shift`, `ctrl` and `alt` joined with `+` (in that order when written), or `none`. The same combination can belong to one view only; a duplicate or invalid key is ignored and the view still loads. Escape (27), Backspace (8) and the plain modifier codes cannot be used.
- `order`: the place inside its group (1 is first). Groups are sorted by `order` alone (ties by id) and renumbered 1, 2, 3 when the file is read. A file written by an earlier version has no `order` lines: each view then gets its place from the old display order (starred views first, then by id), so the lists look the same as before the first time, and the order is written the next time something is saved. Views without an `order` in an otherwise numbered file go after the last numbered one.

**Old files** (the nine-slot format, with `slotN.` lines and no `format=` line) are converted when they are loaded: slot N becomes a view with key N, and its kind (no kind means external), name, favourite and view command are kept. A slot with no name gets `Cockpit view N` or `Outside view N`. The old file is not touched when it loads. It is rewritten in the new format the next time something is saved (a save, rename, favourite, key change or delete); `Log.txt` has one line `converted N view(s) from the old slot file format` when this happens. Once rewritten, older versions cannot read the file, so keep a copy if you want to go back.

## Commands

| Command | Does |
|---------|------|
| `mykam/go_slot_N` (N = 0 to 9) | go to the view that has key N (none: one log line `slot N is empty`) |
| `mykam/save_slot_N` (N = 0 to 9) | save the current view into the view that has key N (name and favourite stay), or create a new view with key N when none has it. Refused in circle, tower and the other no-save views. |
| `mykam/next` | go to the next view in group order (Internal, then External, each in the order you set), wraps |
| `mykam/previous` | go to the previous view in the same order |
| `mykam/release` | give the camera back to X-Plane |
| `mykam/toggle_window` | show or hide the main window (showing it also turns MyKam on) |
| `mykam/toggle_active` | turn MyKam off (camera back to X-Plane, window and quick bar hidden, custom keys freed, go and save ignored with one log line) or on again; also Plugins > MyKam > MyKam on |
| `mykam/toggle_group` | switch the quick bar between the Internal and External lists, like clicking the other segment (holds until the kind of view changes) |

Bind keys to the `mykam/` command names.

### Suggested keys

| Command | Key |
|---------|-----|
| `mykam/toggle_window` | Numpad Enter |
| `mykam/next` | Numpad + |
| `mykam/previous` | Numpad - |

These are only suggestions: pick any keys that are free in your own X-Plane keyboard settings (Settings > Keyboard, search `mykam`). The binding screen warns you if a key is taken. Not checked: how X-Plane spells these three keys in its key profile files, and whether X-Plane treats Numpad Enter as the same key as Return.

## In-sim test checklist (UI-01 to UI-125)

Set `Num Lock` on. Use a fresh aircraft folder or delete its `cameras\<aircraft>.slots.txt` first when a step says "empty". Log is `Log.txt` in the X-Plane folder; lines start with `[MyKam]`. This checklist has not been run in X-Plane yet.

| ID | Do | Pass when |
|----|----|-----------|
| UI-01 | Start X-Plane, load an aircraft | No MyKam window is open. Plugins menu has `MyKam` with Show window and Hide window. |
| UI-02 | Plugins > MyKam > Show window | The `MyKam` window opens as a dark glass panel with rounded corners, a thin cyan border and a soft shadow, a header (camera icon, `MyKam`, status pill, three icons), the sidebar and the content. No X-Plane title bar, no flicker, text is sharp. |
| UI-03 | Plugins > MyKam > Hide window | The window closes. Show it again: it comes back where it was. |
| UI-04 | Bind and press `mykam/toggle_window` | Toggles the window. Pressing it with the window open closes it. |
| UI-05 | Close the window with the header close icon (cross), then Show window | It reopens normally. |
| UI-06 | Look at the header with no view active | `X-Plane in control`. |
| UI-07 | Click the sidebar items | Internal, External, X-Plane views and Settings each show their page with title and subtitle; the selected item has the cyan bar on the left; switching is instant. A divider sits between External and X-Plane views. |
| UI-08 | Internal and External pages, aircraft with no saved views | Each shows a friendly empty text and the `+ Save current view` button. The sidebar badges read 0. Previous and Next are greyed out. |
| UI-09 | Free camera (C), place it, open External, click `+ Save current view` | A row `Outside view 1` appears with a Free camera chip, an outline star, a dash as key badge and a Go pill. The External badge reads 1. Log: `saved camera as 'Outside view 1'`. |
| UI-10 | 3D cockpit (Shift+0), look around, open Internal, click `+ Save current view` | A row `Cockpit view 1` with a Cockpit chip. The Internal badge reads 1. |
| UI-11 | Chase view (Shift+8), External, `+ Save current view` | A row `Outside view 2` with a Chase chip. |
| UI-12 | In circle view (Shift+4) look at both save buttons | Both are dimmed. Hover shows which view to switch to. Clicking does nothing; no log line. |
| UI-13 | In the cockpit view look at the External save button, in the free camera view look at the Internal one | The one for the other group is dimmed with a tooltip (`Switch to the free camera or the chase view...` or `Switch to a cockpit view...`). |
| UI-14 | Click **Go** on `Outside view 1` | The camera glides there. The row gets the cyan border and glow; the status pill reads `Outside view 1 · Free camera` with a cyan dot. |
| UI-15 | Click an empty part of the `Cockpit view 1` row (not a button) | Goes to the cockpit view; that row is now the highlighted one. |
| UI-16 | Click the star on a row | Star turns amber and the view appears as a chip in the quick bar (for the group you are in). The row does not move. Click again: outline again and the chip goes. |
| UI-17 | Mark a view favourite, restart X-Plane, open the window | It still has its star and is still a chip in the quick bar, in the same place. |
| UI-18 | Right-click a row | A small menu: Edit..., Update to current view (only when the current view fits), Show in quick bar (with a check mark when starred), Move up, Move down, Delete.... It stays inside the window. |
| UI-19 | Edit..., type `Left window` in Name, Save | The row shows `Left window`; the status pill uses it when the view is active. File has `view1.name=Left window`. |
| UI-20 | Edit..., try to type 40 characters in Name | Stops at 32. |
| UI-21 | Edit..., try an accented letter or paste text with one in Name | Those characters are not accepted. |
| UI-22 | Edit..., clear the Name | A red `Enter a name.` appears under the field and Save is dimmed. Typing a name enables Save again. |
| UI-23 | Click the Name field, type, then press an X-Plane key (for example C) after clicking Save or Cancel | While typing, keys go to the field (X-Plane does not react). After Save or Cancel the X-Plane keys work again at once. |
| UI-24 | Move the free camera, right-click `Outside view 1` > Update to current view | The view now holds the new camera; name, label, key, star and place are unchanged. In a cockpit view the entry is not in the menu of an External row. |
| UI-25 | Right-click a row > Edit... > Key > Numpad slot > pick 3 > Save | The row's key text reads `Num 3`; the chip's tooltip says `key: Num 3`; file has `viewN.hotkey=3`. The grid is laid out like a numeric keypad. |
| UI-26 | Give numpad slot 3 to a second view the same way | The grid button for 3 shows the first view's name before you click. After Save, the second view has the key and the first shows a dash; there is still one view with slot 3. |
| UI-27 | Edit... > Key > None > Save | The key text goes back to a dash. The view still works from the list. |
| UI-28 | Right-click a row > Delete... > No | Nothing is deleted. |
| UI-29 | Right-click a row > Delete... > Yes | The row is gone, the sidebar badge goes down by one, the view's lines are gone from the file. Saving a new view afterwards does not reuse the old number in the file. |
| UI-30 | Right-click the empty space under the rows | Nothing opens. |
| UI-31 | Click **Next** repeatedly | Goes through the Internal views (in list order), then the External views, then wraps to the first Internal one. |
| UI-32 | Click **Previous** repeatedly | The same backwards. With no active view (after Release) Previous goes to the last External view, Next to the first Internal one. |
| UI-33 | Click **Release** | The camera is given back. The status pill shows a green dot and `X-Plane in control`, no row highlighted. |
| UI-34 | Drag the Transition time slider and let go | Value changes in 0.1 steps; `mykam.ini` is rewritten once, when you let go (not while dragging). The next Go uses it. |
| UI-35 | Set the slider to 0 | Go is an instant cut. |
| UI-36 | X-Plane views page: press each of the ten tiles | X-Plane switches to 3D cockpit, free camera, chase, circle, tower, runway, linear spot, still spot, ride-along, wingman. Any camera MyKam held is released first. The pressed tile is highlighted; the status pill returns to `X-Plane in control`. |
| UI-37 | Read the note under the tiles | It says saving is available in cockpit, free camera and chase views. |
| UI-38 | Settings page: the transition slider | Same value as the footer slider; changing either changes both. |
| UI-39 | After the glide (segmented control): Free camera, Hold, Saved view | The next Go ends in X-Plane's free camera, keeps MyKam's mouse and key control, or the view's own type. `mykam.ini` `handoff_view` follows. |
| UI-40 | Switch the Show quick bar toggle off | The quick bar disappears at once. `show_quick_bar = 0` in the file. |
| UI-41 | Switch it on again, set position Top, then Bottom | The bar returns and moves to the top, then the bottom centre. `quick_bar_position` follows. |
| UI-42 | Add a comment line and `my_note = 1` to `mykam.ini`, change a setting in the window, open the file | Your comment and `my_note` are still there; the setting line changed. |
| UI-43 | Restart X-Plane | The settings are as you left them. |
| UI-44 | Make the window narrow (drag the right edge, down to its minimum) | Rows stay readable: the name shortens with `...` and the type chip is dropped when there is no room; nothing overlaps. Enlarge it again: the chip returns. |
| UI-45 | Pop the window out with the header pop-out icon and use it | Rows, buttons and the context menu still work in the separate window. |
| UI-46 | Quick bar: look at it in a cockpit view with two cockpit views saved, one with key 3 | The `Internal` segment is solid cyan with dark text. Only the starred views show as chips, in window order; the active view's chip is solid cyan. A chip has no number badge; the key is in its tooltip. |
| UI-47 | Click a chip | Goes to that view. Pressing the key 3 command (`mykam/go_slot_3`) does the same. |
| UI-48 | Right-click a chip | A glass popup with Update to current view (first; missing when the current view does not fit the group), Edit..., Move left, Move right, Remove from quick bar and Delete.... Update and Delete > Yes/No behave as in the window; the popup closes when the mouse leaves it. |
| UI-49 | Hover a chip for a moment | A tooltip with the full name, the type and the key text (or `none`) appears. |
| UI-50 | Click the Cockpit, Free, Chase, Release and Menu icon buttons on the quick bar | X-Plane switches to the cockpit, free camera, chase view; Release gives the camera back; Menu toggles the main window. |
| UI-51 | Click on the 3D view next to the quick bar and on the sim's own menus | They work as usual: the bar takes clicks inside its own rectangle only. |
| UI-52 | Bind and press `mykam/next` and `mykam/previous` | They behave like the Next and Previous buttons. With nothing saved the log says `no saved cameras to step through`. |
| UI-53 | Put an old nine-slot file in `cameras` (no `format=` line, some `slotN.name` and `slotN.kind=cockpit` lines) and load the aircraft | Slot N shows up as a view with key N in the right group with its name, star and type; unnamed ones are `Outside view N` / `Cockpit view N`. The log has `converted N view(s)` once and no warning. The file on disk is still the old text. |
| UI-54 | Then change anything (for example mark a favourite) and open the file | It now starts with `format=2`; there are no `slotN.` lines left. |
| UI-55 | Change aircraft | The pages show the new aircraft's views; the status pill is `X-Plane in control`. |
| UI-56 | Open Shift+Ctrl+F (FPS) with the window and bar open, and with them hidden | No visible difference with the window hidden; small cost with both open. Note the numbers. |
| UI-57 | Look at `Log.txt` while using the window | Only lines for actions (saved, renamed, key assigned, deleted, settings saved), none repeating every frame. |
| UI-58 | Plugins > Plugin Admin > disable MyKam, enable it again | The window and bar go away and come back; no crash, the menu has one `MyKam` entry. |
| UI-59 | Take the camera with another plugin or use X-Plane's own view keys while holding a view | The status pill goes to `X-Plane in control`. |
| UI-60 | Resize X-Plane's window or change monitors, then switch the quick bar position | The bar sits at the centre of the main screen again. |
| UI-61 | Drag the header (not the icons) with the mouse | The window follows the mouse and stays where you drop it. It cannot be dragged completely off the desktop. |
| UI-62 | Drag a window edge or corner | The window resizes (the 10 px edge around the glass panel is the grip). Content reflows; it stops at the minimum size. |
| UI-63 | Click minimise, then click it again | The window shrinks to the header only (the sidebar and the page disappear); the second click restores the earlier size. Minimise is not shown while popped out. |
| UI-64 | Pop out, drag the OS window, click the pop-out icon again | The window leaves X-Plane, moves like any OS window, and comes back into X-Plane. Corners are square while popped out. |
| UI-65 | Hover and press the Go pills, the save button, round Previous/Next, Release and header icon buttons | Hover brightens them; the pressed state is darker; disabled buttons (nothing saved, or no fitting view) look dim and do nothing. |
| UI-66 | Click the toggle switch, the segmented controls and drag the slider on the Settings page | Toggle slides and turns cyan; the chosen segment turns cyan; the slider knob follows the mouse and the value shows on the right. Values land in `mykam.ini` (slider once, on release). |
| UI-67 | Look at text and icons at 100 percent and at X-Plane's high-DPI UI scale (if you use one) | Text is Inter, sharp; icons are Material icons, centred in their buttons. Nothing is blurry or double sized. |
| UI-68 | Quit X-Plane, rename `Resources\plugins\MyKam\fonts` to `fonts_off`, start X-Plane, open the window | The window still works with the small built-in font and letters instead of icons. `Log.txt` has exactly one `[MyKam] warning:` line about a font file. Rename it back afterwards. |
| UI-69 | Move the mouse onto and off the quick bar | It fades to about 60 percent when the mouse leaves and returns to full when it comes back. |
| UI-70 | Hover a quick bar chip and a header icon | A small glass-style tooltip appears, with rounded corners. |
| UI-71 | Open the row menu (right-click) and the edit dialog | The menu and the dialog have the same glass look; Save/Cancel/Yes/No are pill buttons; typing and the X-Plane key hand-over behave as in UI-23. |
| UI-72 | Check the frame time (Shift+Ctrl+F) with the window open on each page, then with it hidden | Note the numbers; the drawing should not cost noticeably more than before the restyle (more fonts and custom drawing). |
| UI-73 | Press the key bound to `mykam/save_slot_5` with no view on key 5, then again after moving the camera | The first press creates a view with key 5 (it appears in the right group); the second replaces its camera and keeps name and favourite. |
| UI-74 | Press the key bound to `mykam/go_slot_5` after key 5 was moved to another view in Assign key... | It goes to the view that has key 5 now. With no view on the key the log has `slot 5 is empty` once. |
| UI-75 | Save views until the aircraft has 99, then click `+ Save current view` and press `mykam/save_slot_N` for a free key | Nothing is added; the log has one line each saying the limit of 99 views is reached. Saving into a key that a view has still works. |
| UI-76 | Star one later view in each group and step with Next from the start | Stars do not change the step order: the Internal views in list order, then the External views in list order. Move a view up in the list and Next follows the new order. |
| UI-77 | Hover the dimmed save button, then switch X-Plane to the view named in the tooltip | The button becomes enabled without reopening the window. |
| UI-78 | Create a view in the free camera, switch to the cockpit, right-click that External row | `Update to current view` is not in the menu. In the free camera it is. |
| UI-79 | Release, switch to the circle view and look at the quick bar + button | The + button is dimmed in both groups; its tooltip says which view to switch to. Nothing can be saved there. |
| UI-80 | Delete the view that is active | The row disappears and the status pill goes to `MyKam is holding the camera` (if MyKam holds the camera) or `X-Plane in control`. |
| UI-81 | Delete a view, restart X-Plane, open the window | The view stays gone and its key (if it had one) is free. |
| UI-82 | Edit the file by hand: give one view `view2.x=abc` and another a name of 40 letters | The first view is skipped (one warning in the log); the second loads with a default name. |
| UI-83 | Look at the count badges after saving into one group and deleting from the other | They show each group's count at once. |
| UI-84 | Hover a button in the Numpad slot grid of the edit dialog for a slot another view has | A tooltip names that view and says saving moves the key. |
| UI-85 | Rename a view to a name already used by another view | Allowed; both rows show the name. |
| UI-86 | Switch X-Plane between a cockpit view and the free camera / chase / circle | The quick bar switch follows: `Internal` in the cockpit, `External` outside; the chips change to that group. |
| UI-87 | In the cockpit, click `External`; then change view inside the cockpit (look around, glance) | The bar keeps showing External. Switch to an outside view and back to the cockpit: it follows again (`Internal`). |
| UI-88 | Click Go on an External view while in the cockpit (transition on) | The bar shows External while MyKam holds or hands over the outside camera. Go to an Internal view from outside: it shows Internal during the head glide. |
| UI-89 | Bind `mykam/toggle_group` and press it | Same as clicking the other segment; the choice holds until the kind of view changes. |
| UI-90 | Save 15 or more views in one group; look at the bar | The chip area stops at about 60 percent of the screen width; left and right arrows appear; the mouse wheel over the bar scrolls the chips; the arrows scroll too and dim at the ends. |
| UI-91 | Delete views until few remain | The bar shrinks to fit; it stays centred. Remove the stars of a group: `Star a view to show it here` shows next to the + button. |
| UI-92 | In the free camera press + with External shown; then in a cockpit view with Internal shown | A new starred view appears as a chip at the end of the shown group (and as a starred row in the window). With the other group shown (or in circle view) the + button is dimmed and its tooltip says which view to switch to. |
| UI-93 | Use a view with a name longer than 14 characters | The chip shows the first characters and `...`; the tooltip shows the full name. |
| UI-94 | Set the quick bar position to Top, right-click a chip | The bar stays at the top edge; the popup opens below it and works (clicks reach it). Same at the bottom: the popup opens above. |
| UI-95 | Click on the 3D view just beside the bar, and above/below it after the popup closes | The clicks reach X-Plane as usual; the bar's window is back to the bar's own height. |
| UI-96 | Watch Log.txt and the frame time while the bar is shown with many chips and while scrolling | No log lines from the bar; no visible stutter (the window geometry only changes when the bar's size changes). |
| UI-97 | Edit a chip's view from its popup (Edit...) using a long name | The chip's text is cut at 14 characters; renaming keeps its position in the group order. |
| UI-98 | Right-click a chip while the main window is hidden, choose Edit... | The main window is shown and the edit dialog is open in it, the window dimmed behind. Collapsed (minimised) windows expand first. |
| UI-99 | In the edit dialog type `ab` in Quick bar label | The preview chip shows a round `AB` badge and the name; typing a third character or a symbol is not accepted. Save, and the chip in the bar and the row in the window show the `AB` badge. File has `viewN.label=AB`. |
| UI-100 | Clear the label and Save | The chip and the row show the type icon again; the `label` line is gone from the file. |
| UI-101 | Key > Custom key, click the capture field, press Ctrl+Shift+W | The field shows `Ctrl+Shift+W`. Save: the row's key text and the chip's tooltip read `Ctrl+Shift+W`. File has `viewN.key=vk:87,flags:shift+ctrl`. |
| UI-102 | With the dialog closed, hold Ctrl+Shift and press W | X-Plane goes to the view (same as Go). Plugins > ... or the Keyboard settings list a hot key described `MyKam: <view name>`. |
| UI-103 | Click the capture field, press Esc; then click it and press Backspace | Esc leaves the old key as it was and ends the capture (the dialog stays open); Backspace clears the key (field shows `Click, then press a key`). With a cleared key in Custom mode the Save button is dimmed with `Press the key you want to use.` |
| UI-104 | Assign the same custom key to a second view | The dialog shows `Used by <first view>. Saving moves the key here.`; after Save the first view has no custom key and pressing the key goes to the second view. |
| UI-105 | Read the muted note under the capture field | It says X-Plane does not tell plugins which keys it uses and that a key bound in X-Plane may stop working while MyKam uses it. |
| UI-106 | Bind a key to a view that X-Plane uses (for example a view-change key), press it | Note which action happens (X-Plane's, MyKam's or both) and write it down: this is the case the note warns about. Remove the custom key afterwards. |
| UI-107 | Edit the name, label, key and star and press Save; open the file | One change to the file with all four fields. Cancel instead: nothing changes. |
| UI-108 | Change the aircraft to one with custom keys, then back | The old aircraft's keys stop working and the new aircraft's keys work; the hot key list shows only the current aircraft's `MyKam:` entries. Disable the plugin: all of them are gone. |
| UI-109 | In a list with 4 or more views, drag a row onto another row | A cyan outline marks the target; on release the dragged view takes the place of the target; the other rows shift by one; the order stays after restarting X-Plane. Dragging onto the other group's list or the sidebar does nothing. |
| UI-110 | Row menu > Move up and Move down | The row moves one place; the entries are dimmed for the first and the last row. |
| UI-111 | In the quick bar drag a chip left and right over other chips | The chip takes the place of the chip it is dropped on; the order in the window list follows. Scrolling the bar while dragging is not needed for up to about ten chips. |
| UI-112 | Chip menu > Move left / Move right with an unstarred view between two starred ones | The chip jumps past the neighbour chip (the unstarred view in between is passed over too); the entries are dimmed at the ends. |
| UI-113 | Chip menu > Remove from quick bar | The chip disappears; the view is still in the window list with an outline star. |
| UI-114 | Click + in the quick bar | A new view appears in the bar and in the window list with a filled star, at the end of both. |
| UI-115 | Move the camera, chip menu > Update to current view | The view holds the new camera; name, label, key and place stay. The entry is the first one in the menu and is missing when the current view does not fit the group. |
| UI-116 | Open an aircraft file written by the previous version (no `order`, `label` or `key` lines) | The lists look as before (starred views first, then in age order); the stars now decide what the bar shows. After any change the file has `order` lines. |
| UI-117 | Watch `Log.txt` while editing, dragging and registering keys | One line per action (`edited 'Name'`, `moved 'Name'`, key assigned); no line per frame. |
| UI-118 | Give two or three views a 2 letter label (for example `PI`, `FO`, `TW`), star them, look at the bar at the normal UI size and with X-Plane's UI scaled to 200 percent | Every label chip is a clean circle with a visible gap to its neighbours and to the + button; no circle touches or overlaps another, none is cut off at the left or right end of the bar or window. Hover and active chips stay inside their circle. |
| UI-119 | Settings > Quick bar buttons: look at the list | Ten rows with icon, name and a toggle. Three are on (3D cockpit, Free camera, Chase) and first; each has an up and a down arrow, the first up and the last down are dimmed. |
| UI-120 | Turn Circle, Tower and Wingman on | The three buttons appear in the quick bar at once, at the end, each with its icon (`360`, tower, group); the bar gets wider and stays centred; no click on the window is needed. `mykam.ini` has `quick_bar_views = cockpit,free,chase,circle,tower,wingman`. |
| UI-121 | Hover each new button; click one | The tooltip says `X-Plane view: <name>`. The click switches X-Plane to that view and releases any camera MyKam holds; that button turns solid cyan and the previous one goes dark. Go to a saved view: no X-Plane button is cyan. |
| UI-122 | Use the arrows: move Wingman up twice, Cockpit down once | The order in the list and in the quick bar changes at once and follows the arrows; the file's list has the same order. |
| UI-123 | Turn every view off | The bar shows only the switch, the chips, +, Release and Menu, centred, with no gap or overlap; the file has `quick_bar_views =`. Turn one on again: it comes back. |
| UI-124 | Edit `mykam.ini` by hand: `quick_bar_views = Tower, foo, tower, chase`, restart X-Plane | The bar has Tower and Chase in that order; `Log.txt` has one `[MyKam] warning:` line about the unknown name `foo`. With the key missing the bar has Cockpit, Free and Chase. |
| UI-125 | Open the X-Plane views page after changing the bar buttons | The page is the same ten tiles as before, in the same order; the bar settings do not change it. |

## Known limits

- Hover and clicks outside the window rectangle are not delivered by X-Plane; menus and the key grid are kept inside the window for that reason. The quick bar's right-click popup gets room by making the bar's window taller (and wider if needed) while it is open.
- The quick bar is laid out and measured from the first drawn frame, so its X-Plane window is resized once or twice right after start-up and whenever its content width changes; the bar skips drawing (up to three frames) while its window is narrower than the bar, so one or two frames may show nothing instead of a cut-off bar.
- The main window keeps a 10 px transparent border around the glass panel for the shadow and as a resize grip. X-Plane gives that border to the window too, so clicks in it do not reach the sim.
- The window is resizable by its edges only; there is no drawn resize handle.
- The quick bar is placed when it is shown or its position changes; it does not follow a later resize of X-Plane's window until you toggle it.
- The window is made for the main X-Plane screen and a popped-out OS window. Virtual reality is not supported.
- A converted file cannot be read by the earlier nine-slot version of MyKam.
- X-Plane does not tell plugins which keys it uses, so MyKam cannot warn when a custom key is already bound in X-Plane; both the X-Plane binding and MyKam may react, or only one of them. Pick a key that is free in your key bindings.
- Not checked in the sim yet: that the key callback of a window reports a bare Shift, Ctrl or Alt press (the capture ignores a key code of 0 and the plain modifier codes either way), the drag and drop feel with X-Plane's mouse events, and the dialog height at the minimum window size (it scrolls when it does not fit).
