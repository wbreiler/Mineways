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
- [ ] Go To Location dialog (port `Win/Location.cpp`)

## Export

- [ ] Port Export / Print dialog (`Win/ExportPrint.cpp`, ~1600 lines) to `wxDialog`
- [ ] Wire `File > Export Model` to the ported dialog + `SaveVolume()`
- [ ] Schematic (.schem) import: wire `loadSpongeSchematic` path in `LoadWorldFromDir`
- [ ] Save/restore last export path via `wxConfig`

## Culling Schemes

- [ ] Port `Win/CullingSchemes.cpp` registry calls to `wxConfig` (plist on macOS)
- [ ] Port Culling Scheme editor dialog → `wxDialog` + `wxListCtrl` (replace `CullingStubs.cpp`)

## Persistence (wxConfig)

- [ ] Save/restore: last world path, zoom level, cursor X/Z
- [ ] Save/restore: terrain file path
- [ ] Save/restore: export settings (`ExportFileData`)
- [ ] Save/restore: recent exports submenu (up to 5 entries)

## App Packaging

- [ ] Bundle as `.app` with `Info.plist` and icon
- [ ] Locate `terrainExt.png` relative to bundle `Resources/` directory
- [ ] Custom terrain file picker (`File > Choose Terrain File`)
