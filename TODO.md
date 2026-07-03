# Mineways wxWidgets Port — TODO

Items are roughly ordered by dependency (foundations first).

## Foundation / Rendering

- [x] Core library (blockInfo, nbt, ObjFileManip, MinewaysMap, etc.) compiles for ARM64
- [x] wxWidgets skeleton builds and runs (MinewaysApp + MinewaysFrame + MapPanel)
- [x] Remove redundant `MapPanel::m_bits`; `OnPaint` already reads `gMapBits` via `GetMapBits()`
- [x] Wire "Test Block World" menu item → verify map actually renders (no Minecraft world needed)
- [x] Populate File > "Open World" submenu with worlds found in `~/Library/Application Support/minecraft/saves` (opendir scan)
- [x] Verify pixel format: DrawMap writes R,G,B,0xFF (already RGB — no swap needed); removed incorrect BGR swap
- [x] Add `Mac/mineways` binary to `.gitignore`

## Map Interaction

- [x] Mouse hover → status bar block name + X/Y/Z (IDBlock on `EVT_MOTION` when not dragging)
- [x] Selection rectangle: right-click drag to set export region; draw highlight overlay on map
- [x] Bottom slider: wire to `gTargetDepth` (export floor depth, not just displayed)
- [x] Space bar → if selection active, snap bottom depth to minimum solid height (`GetMinimumSelectionHeight`)
- [x] Go To Location dialog (`Mac/LocationDialog.cpp` — Ctrl+G; replaces Win/Location.cpp)

## Export

- [x] Port Export / Print dialog → lean `ExportDialog.cpp` (~200 lines); wires to `SaveVolume()`
- [x] Wire `File > Export Model` to the ported dialog + `SaveVolume()`
- [x] Schematic (.schem) import: `LoadSchematicFile()` wired into `OnOpenFile`; handles both legacy and Sponge
- [x] Save/restore last export path via `wxConfig`

## Culling Schemes

- [x] Port `Win/CullingSchemes.cpp` → `Mac/MacCullingSchemes.cpp` with `wxConfig` persistence
- [x] Port editor dialog → `wxDialog` + `wxCheckListBox` with name/filter/Hide-All/Show-All

## Persistence (wxConfig)

- [x] Save/restore: last world path, zoom level, cursor X/Z
- [x] Save/restore: terrain file path
- [x] Save/restore: last export path
- [ ] Save/restore: full export settings (`ExportFileData`) — add when options matter
- [ ] Save/restore: recent exports submenu (up to 5 entries) — add when needed

## App Packaging

- [x] Bundle as `.app` with `Info.plist` and icon (`make app` target)
- [x] Locate `terrainExt.png` relative to bundle `Resources/` directory (GetResourcesDir() with exe-dir fallback)
- [x] Custom terrain file picker (`File > Choose Terrain File`)
