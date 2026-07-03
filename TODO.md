# Mineways wxWidgets Port — TODO

Items are roughly ordered by dependency (foundations first).

## Feature Parity Audit (2026-07-03)

Full audit comparing the Mac port against the Windows Mineways UI (menus,
export dialog, culling, other dialogs, shortcuts, map interaction, file
formats). Core map viewing, basic export, and Culling Schemes are solid;
gaps concentrate in the View menu and the Export dialog. Working through
them in this order — **resume here first**.

### Now / next (in this order)

- [x] Fix dead `OnOpenWorld` handler — added a "Browse for World Folder..."
      item to the Open World submenu.
- [x] Wire up the View menu — added `&View` menu with Select All, Undo
      Selection, Jump to Spawn/Player, Information dialog, View Nether/End,
      and all 8 render-mode toggles (show all objects, show biomes,
      elevation shading, lighting, cave mode, hide obscured, transparent
      water, map grid), plus Zoom out further. All wired to the existing
      `gOptions.worldType` bits and shared renderer — no rendering logic
      was new. Spawn/player coords are now kept in globals (`gSpawnX/Y/Z`,
      `gPlayerX/Y/Z`) instead of being discarded after world load.
- [x] Select All (Ctrl+A) — implemented alongside the View menu work
      (`MinewaysFrame::OnSelectAll`), matching `Win/Mineways.cpp:2108-2145`
      including the minimum-depth calculation for the non-schematic case.

**Not yet done from this batch** (lower priority, didn't block the above):
Reload World, Focus View accelerator polish, per-key accelerators beyond
what's listed above matching Windows' full accelerator table exactly.

### Export dialog expansion — done (2026-07-03)

`Mac/ExportDialog.cpp` now operates directly on `ExportFileData` (the same
struct `Win/ExportPrint.cpp` uses) instead of a lossy separate settings
struct, with a `wxNotebook` of 5 tabs (General/Materials/Sizing/3D Print
Prep/Advanced) covering essentially every field: rotation (0/90/180/270°),
all 4 model-sizing strategies with the shared `gMtlCostTable`/
`gUnitTypeTable` dropdowns, all 3D-print prep ops (fill bubbles, seal
entrances/tunnels, connect parts/corner tips/edges, delete floaters, hollow
+ superhollow, melt snow, export-all + fatten), 5 material modes, texture
channels, biome/leaves/borders/composite toggles, ZIP + keep-files, and the
full advanced set (separate types, individual-block USD instancing,
material-per-family, split-by-type, custom material, export MDL, doubled
billboards, decimate). Two preset buttons ("Load Rendering Defaults" /
"Load 3D-Printing Defaults") reproduce Windows' `initializeViewExportData`/
`initializePrintExportData` split, since Mac has one unified export menu
item instead of two. `MinewaysFrame.cpp` gained a `BuildExportFlags()`
that ports the `exportFlags` bitmask translation from
`Win/Mineways.cpp:5296-5542` line-for-line, so the dialog's choices have
the same effect on `SaveVolume()` as on Windows.

Verified via a standalone headless harness (built and torn down during
this session — not checked in) exercising `InitViewExportData`/
`InitPrintExportData` through real `SaveVolume()` calls for OBJ, STL, and
USD; this caught and led to fixing a real pre-existing cross-platform bug:
`Win/ObjFileManip.cpp` hardcoded `L"\\"` as a path separator in 3 places
(materials/tile subdirectory construction), producing directories literally
named `tex\` on macOS instead of a `tex/` subdirectory. Fixed by using the
already-existing but previously-unused `gSeparator` (set via
`SetSeparatorObj`, called with `"/"` on Mac and `"\\"` on Windows) at all
3 sites — Windows behavior unchanged, Mac now creates correct subdirectories.

**Known simplification** (`ponytail:` — ceiling is minor UX inconsistency,
not incorrect output): per-file-type fields (block size, hollow thickness,
physical material, etc.) don't live-reload when the user changes the File
Type dropdown mid-dialog — they're loaded once at dialog-open time for
whatever file type was current, and written back to whichever file type is
selected at OK time. Windows reloads these live. Upgrade path: bind
`m_typeChoice`'s selection-changed event to re-run `LoadFromEfd()`.

Not done: progress bar / cancel button during export (Windows shows one;
Mac still passes `nullptr` for the progress callback — safe, confirmed via
`UPDATE_STATUS`'s null-check, but no progress UI for long exports).

### Backlog (lower priority / optional)

- [ ] File menu: Reload World, Choose Terrain File history submenu,
      Download Terrain Files, Import Settings (Ctrl+I), Export Map (2D
      image, Ctrl+M), Repeat Export (Ctrl+X)
- [ ] Sketchfab publish integration (Upload/Publish dialogs) — no Mac
      equivalent at all
- [ ] World Information dialog (name/version/spawn/player coords)
- [ ] About box: match Windows content (icon, version, copyright,
      Minecraft-version compatibility line, credits, website link)
- [ ] Help menu: keyboard shortcuts reference (F1), troubleshooting,
      documentation, report a bug, "give more export memory"
- [ ] Remaining keyboard accelerators, added alongside each feature above
      as it lands (F2-F8, single-letter view toggles, etc.)

### At parity (no work needed)

- Culling Schemes dialog (list + editor) — Mac matches or exceeds Windows
  (adds Hide All/Show All)
- File format support — all 9 export `FILE_TYPE_*` covered; schematic +
  Sponge `.schem` import both wired

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
