# Changelog

All notable changes to MyKam are listed here. The format follows Keep a Changelog.

## [0.9.1-beta] - 2026

### Added
- Numpad slot 0: `mykam/go_slot_0` and `mykam/save_slot_0`, and a wide 0 key in the edit dialog's keypad.
- `mykam/toggle_active` and Plugins > MyKam > MyKam on: turn MyKam off (camera back to X-Plane, window and quick bar hidden, custom keys freed, go and save ignored) and on again.
- Updates through SkunkCrafts Updater: the MyKam folder carries `skunkcrafts_updater.cfg`, so the updater finds it and keeps it current. Your `mykam.ini` is never replaced.

## [0.9.0-beta] - 2026

First public beta.

### Features
- Saved views per aircraft: up to 99 cockpit views (Internal group) and outside views (External group: free camera and chase), kept next to the aircraft's `.acf` in `mykam_views.txt`.
- Smooth eased transitions between views, 0 to 10 seconds (default 1.5), with heading taking the short way and a new switch blending from where the camera is.
- Hand-off after an outside move: to the X-Plane view type the view was saved in (chase and so on), to the X-Plane free camera, or hold the camera in MyKam (`handoff_view`).
- Mouse orbit and wheel dolly and X-Plane's own view movement keys while MyKam holds an outside camera.
- Main window (Plugins > MyKam > Show window) with Internal, External, X-Plane views and Settings pages; rename, reorder, label, star, replace and delete views.
- Quick bar with starred views, labels, X-Plane view buttons (cockpit, free, chase, circle, tower, runway, spots, ride-along, wingman), Release and Menu.
- Custom keys per view (any key with Shift, Ctrl and Alt) next to the numpad slots 1 to 9.
- Commands `mykam/save_slot_N`, `mykam/go_slot_N`, `mykam/release`, `mykam/toggle_window`, `mykam/next`, `mykam/previous`, `mykam/toggle_group`.
- Settings file `mykam.ini`, written back from the window while keeping comments and unknown lines.

### Known limitations
- Windows 64-bit only. Not tested on Linux or macOS.
- VR is untested.
- Custom keys can conflict with X-Plane key bindings (X-Plane does not tell plugins which keys it uses).
