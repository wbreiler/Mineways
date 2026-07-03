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

### Medium-priority items — done (2026-07-03)

- [x] Export progress bar: `Mac/MinewaysFrame.cpp` now passes a real
      `ExportProgressCB` (was `nullptr`) into `SaveVolume()`, driving a
      `wxProgressDialog` shown for the duration of the export. No
      `wxPD_CAN_ABORT` — verified Windows has no mid-export cancellation
      mechanism either (`updateProgress` just paints a status-bar gauge;
      nothing in `ObjFileManip.cpp` polls a cancel flag), so a Cancel
      button would be a false promise. Matches Windows' `-999.0f` sentinel
      convention (update message only, leave the bar position alone).
- [x] Full `ExportFileData` persistence: `SaveExportFileData`/
      `LoadExportFileData` hex-encode/decode the whole struct into
      `wxConfig` (same idiom as `MacCullingSchemes.cpp`'s `culled[]`
      persistence), guarded by a stored `sizeof(ExportFileData)` so a
      struct-layout change from a future build can't corrupt-load into a
      live struct — falls back to the `InitViewExportData` preset instead.
      Verified with a standalone round-trip test (byte-identical via
      `memcmp`, built and torn down during development, not checked in).

**Recent Exports turned out to be a different feature than assumed** —
traced through `Win/Mineways.cpp`'s `IDM_RECENT_EXPORT_BASE` handler
(~line 1923-1971): clicking a recent-export entry doesn't re-run an export
to that path, it re-*imports*/reopens that file (as a world if it's a
schematic, otherwise via the same machinery as File > Import Settings).
That machinery (`runImportOrScript`, `gImportFile`) doesn't exist on Mac at
all yet, so implementing this properly means building Import Settings
first. Left in the backlog below rather than half-implementing a
"re-export" shortcut that isn't what Windows actually does.

### Small polish batch — done (2026-07-03)

- [x] About box: matches `Win/Mineways.rc`'s `IDD_ABOUTBOX` static text
      verbatim (version, copyright, "works with Minecraft 1.4-1.19",
      minutor credit, mineways.com link). Skipped the `IDI_MINEWAYS` icon
      bitmap itself — wx doesn't decode `.icns` out of the box and it's
      cosmetic only.
- [x] Help menu: keyboard (F1), troubleshooting, documentation, report a
      bug (all `wxLaunchDefaultBrowser` to the same URLs Windows'
      `ShellExecute` calls use), plus "Give more export memory!" as a
      checkable item wired to `gOptions.moreExportMemory` +
      `MinimizeCacheBlocks()` (shared code, already compiled in).
- [x] File menu: Reload World (`R`, reloads `gWorldGuide.world` via the
      existing `LoadWorldFromDir`), Download Terrain Files (browser link),
      Repeat Export (`Ctrl+X`, re-runs the last export's `gEfd`/
      `gExportPath` without reopening the dialog — required extracting a
      `RunExport()` out of `OnExportOBJ` so both share it).
- [x] Added the one missing accelerator found by diffing against
      `Win/Mineways.rc`'s `IDC_MINEWAYS ACCELERATORS` table: `Ctrl+T` for
      Choose Terrain File.

### Follow-up batch — done (2026-07-03)

- [x] Jump to Model (F4) — traced `Win/Mineways.cpp:2481-2500` and it's
      simpler than the name suggests: centers the view on the *current
      selection's* midpoint (`(minx+maxx)/2, (minz+maxz)/2`), not a
      tracked "last exported model." `OnJumpModel` matches that exactly.
- [x] Export Map (Ctrl+M) — ports `Win/Mineways.cpp`'s `saveMapFile()`:
      re-renders the selected region fresh at the current zoom via
      `DrawMapToArray` (not a screenshot of the live buffer) and writes it
      with `writepng`. Both are shared, platform-independent code already
      compiled into the Mac binary. Verified with a standalone headless
      test (`DrawMapToArray` + `writepng` round-trip, confirmed a valid
      decodable PNG at the expected dimensions; built and torn down
      during development, not checked in).
- [x] "Choose Terrain File **history**" was a misreading on my part —
      traced `Win/Mineways.cpp`'s `loadTerrainList()` and it's not a
      recently-used list at all: it scans the app directory for
      `terrainExt*.png` files shipped alongside the executable (alternate
      texture packs), excluding the `_n`/`_r`/`_m`/`_e` PBR-channel
      suffix files. Implemented as `ScanTerrainFiles()` (same pattern as
      `ScanWorldSaves`) populating a "Choose Terrain File" submenu with
      `[default]` + whatever's found. Also fixed a small pre-existing gap
      while touching this code: `OnChooseTerrainFile` (the browse dialog)
      never redrew the map after picking a new file — now it does, along
      with the two new menu paths.
- `/` and `?` Help aliases skipped — F1 already covers Help: keyboard and
  two extra single-character global accelerators isn't worth the
  collision risk for a purely redundant binding.

### Backlog (lower priority / optional)

- [ ] Import Settings (Ctrl+I) — prerequisite for Recent Exports, see above
- [ ] Recent Exports submenu (`IDM_RECENT_EXPORT_BASE`, up to 5 entries) —
      blocked on Import Settings, see above
- [ ] Sketchfab publish integration (Upload/Publish dialogs) — no Mac
      equivalent at all — needs a Sketchfab account/API token, likely out
      of scope for unattended work

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
- [x] Save/restore: full export settings (`ExportFileData`)
- [ ] Save/restore: recent exports submenu (up to 5 entries) — blocked on Import Settings

## App Packaging

- [x] Bundle as `.app` with `Info.plist` and icon (`make app` target)
- [x] Locate `terrainExt.png` relative to bundle `Resources/` directory (GetResourcesDir() with exe-dir fallback)
- [x] Custom terrain file picker (`File > Choose Terrain File`)
