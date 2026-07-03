// MinewaysFrame.cpp — wxWidgets main window, replacing Win32 Mineways.cpp
// Global state mirrors the static variables in Win/Mineways.cpp.
#include <wx/wx.h>
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include "MinewaysFrame.h"
#include "MapPanel.h"
#include "LocationDialog.h"
#include "ExportDialog.h"
#include "MacCullingSchemes.h"

// Win32 shims + project headers (stdafx.h routes to compat.h on non-WIN32)
#include "stdafx.h"
#include "CullingSchemes.h"   // applyCullingScheme/isBlockCulled declarations

// ─── Globals (mirror of Win/Mineways.cpp static globals) ────────────────────
static Options gOptions = {
    0,
    BLF_WHOLE | BLF_ALMOST_WHOLE | BLF_STAIRS | BLF_HALF | BLF_MIDDLER |
    BLF_BILLBOARD | BLF_PANE | BLF_FLATTEN | BLF_FLATTEN_SMALL,
    0x0, false, INITIAL_CACHE_SIZE, nullptr
};
static WorldGuide  gWorldGuide;
static int         gVersionID       = 0;
static int         gMinecraftVersion = 0;
static int         gMaxHeight       = INIT_MAP_MAX_HEIGHT;
static int         gMinHeight       = 0;
static double      gCurX            = 0.0;
static double      gCurZ            = 0.0;
static double      gCurScale        = 1.0;
static int         gCurDepth        = INIT_MAP_MAX_HEIGHT;
static BOOL        gLoaded          = FALSE;
static int         gHitsFound[4]    = {0,0,0,0};
static int         gTargetDepth     = SEA_LEVEL;
static BOOL        gHighlightOn     = FALSE;
static int         gStartHiX        = 0;
static int         gStartHiZ        = 0;
static wchar_t     gSelectTerrainPathAndName[MAX_PATH_AND_FILE];
static wchar_t     gSelectTerrainDir[MAX_PATH_AND_FILE];
static wchar_t     gWorldPathDefault[MAX_PATH_AND_FILE];
static wchar_t     gPreferredSeparatorString[2] = { L'/', 0 };

// pixel buffer for map (owned here; MapPanel reads it)
static unsigned char* gMapBits  = nullptr;
static int            gMapWidth  = 0;
static int            gMapHeight = 0;

// Scanned world list (parallel to gWorlds in Win/Mineways.cpp)
static wxString gWorldDirs[MAX_WORLDS];
static wxString gWorldDisplayNames[MAX_WORLDS];
static int      gNumWorlds = 0;

// Persistent export settings (survive across dialog invocations)
static ExportSettings gExportSettings = { 0, {}, 0,0,0,0,0,0, 2, true, false };

// The singleton frame (so MapPanel event handlers can reach it)
MinewaysFrame* gFrame = nullptr;

// ─── Helpers ─────────────────────────────────────────────────────────────────
static void splitPath(const wchar_t* pathAndName, wchar_t* dir, wchar_t* name)
{
    const wchar_t* slash = wcsrchr(pathAndName, L'/');
    if (!slash) slash = wcsrchr(pathAndName, L'\\');
    if (slash) {
        if (dir) { wcsncpy(dir, pathAndName, slash - pathAndName); dir[slash - pathAndName] = 0; }
        if (name) wcscpy(name, slash + 1);
    } else {
        if (dir)  wcscpy(dir, L".");
        if (name) wcscpy(name, pathAndName);
    }
}

// Progress callback stub (no UI progress for now)
static void progressCB(float /*p*/, wchar_t* /*buf*/) {}

// Returns 0 on success, non-zero on error (mirrors Win loadSchematic / loadSpongeSchematic).
static int LoadSchematicFile(const wchar_t* pathAndFile, bool isSponge)
{
    CloseAll();

    // Free any prior schematic data
    free(gWorldGuide.sch.blocks); gWorldGuide.sch.blocks = nullptr;
    free(gWorldGuide.sch.data);   gWorldGuide.sch.data   = nullptr;

    if (isSponge) {
        int w = 0, h = 0, l = 0;
        unsigned char* blocks = nullptr;
        unsigned char* data   = nullptr;
        int rv = GetSpongeSchematic(pathAndFile, &w, &h, &l, &blocks, &data);
        if (rv != 1) { free(blocks); free(data); return 100 + (rv == -1 ? 1 : 5); }
        gWorldGuide.sch.width  = w; gWorldGuide.sch.height = h; gWorldGuide.sch.length = l;
        gWorldGuide.sch.numBlocks = w * h * l;
        gWorldGuide.sch.blocks = blocks; gWorldGuide.sch.data = data;
        gVersionID = 2586;
    } else {
        if (GetSchematicWord(pathAndFile, "Width",  &gWorldGuide.sch.width)  != 1) return 101;
        if (GetSchematicWord(pathAndFile, "Height", &gWorldGuide.sch.height) != 1) return 102;
        if (GetSchematicWord(pathAndFile, "Length", &gWorldGuide.sch.length) != 1) return 103;
        gWorldGuide.sch.numBlocks = gWorldGuide.sch.width * gWorldGuide.sch.height * gWorldGuide.sch.length;
        if (gWorldGuide.sch.numBlocks <= 0) return 104;
        gWorldGuide.sch.blocks = (unsigned char*)malloc(gWorldGuide.sch.numBlocks);
        gWorldGuide.sch.data   = (unsigned char*)malloc(gWorldGuide.sch.numBlocks);
        if (!gWorldGuide.sch.blocks || !gWorldGuide.sch.data) {
            free(gWorldGuide.sch.blocks); gWorldGuide.sch.blocks = nullptr;
            free(gWorldGuide.sch.data);   gWorldGuide.sch.data   = nullptr;
            return 105;
        }
        if (GetSchematicBlocksAndData(pathAndFile, gWorldGuide.sch.numBlocks,
                                       gWorldGuide.sch.blocks, gWorldGuide.sch.data) != 1) {
            free(gWorldGuide.sch.blocks); gWorldGuide.sch.blocks = nullptr;
            free(gWorldGuide.sch.data);   gWorldGuide.sch.data   = nullptr;
            return 105;
        }
        gVersionID = 1343;  // MC 1.12.2
    }

    gMinecraftVersion = DATA_VERSION_TO_RELEASE_NUMBER(gVersionID);
    gMaxHeight = MAX_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    gMinHeight = ZERO_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    gWorldGuide.minHeight = gMinHeight;
    gWorldGuide.maxHeight = gMaxHeight;
    return 0;
}

// ─── MapPanel calls this to redraw into gMapBits, then Refresh() itself ──────
// (called from MapPanel.cpp via friend access or by having this function visible)
void RedrawMapIntoBuffer()
{
    if (!gLoaded || !gMapBits || gMapWidth == 0 || gMapHeight == 0) {
        if (gMapBits)
            memset(gMapBits, 0xff, (size_t)gMapWidth * gMapHeight * 4);
        return;
    }
    DrawMap(&gWorldGuide, gCurX, gCurZ,
            gCurDepth - gMinHeight, gMaxHeight,
            gMapWidth, gMapHeight, gCurScale,
            gMapBits, &gOptions, gHitsFound,
            progressCB, gMinecraftVersion, gVersionID);
    // DrawMap writes R,G,B,0xFF per pixel — already correct for wxImage (no swap needed)
}

// Accessors used by MapPanel
double& GetCurX()     { return gCurX; }
double& GetCurZ()     { return gCurZ; }
double& GetCurScale() { return gCurScale; }
BOOL    IsLoaded()    { return gLoaded; }
unsigned char* GetMapBits()  { return gMapBits; }
int     GetMapWidth()        { return gMapWidth; }
int     GetMapHeight()       { return gMapHeight; }
int     GetMinHeight()  { return gMinHeight; }
int     GetMaxHeight()  { return gMaxHeight; }
int&    GetCurDepth()   { return gCurDepth; }
int&    GetTargetDepth() { return gTargetDepth; }
BOOL&   GetHighlightOn() { return gHighlightOn; }
int&    GetStartHiX()    { return gStartHiX; }
int&    GetStartHiZ()    { return gStartHiZ; }
Options*    GetOptions()    { return &gOptions; }
WorldGuide* GetWorldGuide() { return &gWorldGuide; }

// IDBlock wrapper (for status bar lookup)
const char* QueryBlock(int bx, int by, int* mx, int* my, int* mz, int* type, int* dataVal, int* biome)
{
    return IDBlock(bx, by, gCurX, gCurZ,
                   gMapWidth, gMapHeight, gMinHeight, gCurScale,
                   mx, my, mz, type, dataVal, biome,
                   gWorldGuide.type == WORLD_SCHEMATIC_TYPE);
}

// ─── MinewaysFrame ────────────────────────────────────────────────────────────
wxBEGIN_EVENT_TABLE(MinewaysFrame, wxFrame)
    EVT_MENU(ID_OPEN_WORLD,       MinewaysFrame::OnOpenWorld)
    EVT_MENU(ID_OPEN_FILE,        MinewaysFrame::OnOpenFile)
    EVT_MENU(ID_TEST_BLOCK_WORLD, MinewaysFrame::OnTestBlockWorld)
    EVT_MENU(ID_GO_TO_LOCATION,   MinewaysFrame::OnGoToLocation)
    EVT_MENU(ID_CHOOSE_TERRAIN,   MinewaysFrame::OnChooseTerrainFile)
    EVT_MENU(ID_CULLING_SCHEMES,  MinewaysFrame::OnCullingSchemes)
    EVT_MENU(ID_EXPORT_OBJ,       MinewaysFrame::OnExportOBJ)
    EVT_MENU(wxID_EXIT,           MinewaysFrame::OnQuit)
    EVT_MENU(wxID_ABOUT,          MinewaysFrame::OnAbout)
    EVT_SLIDER(ID_SLIDER_TOP,     MinewaysFrame::OnSliderTop)
    EVT_SLIDER(ID_SLIDER_BOT,     MinewaysFrame::OnSliderBot)
    EVT_MENU_RANGE(ID_WORLD_ITEM_BASE, ID_WORLD_ITEM_BASE + MAX_WORLDS - 1,
                   MinewaysFrame::OnWorldMenuItem)
wxEND_EVENT_TABLE()

MinewaysFrame::MinewaysFrame(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Mineways", wxDefaultPosition, wxSize(960, 720))
{
    gFrame = this;

    // Separator always '/' on macOS
    SetSeparatorMap(gPreferredSeparatorString);
    SetSeparatorObj(gPreferredSeparatorString);

    // World guide: start unloaded
    gWorldGuide.type = WORLD_UNLOADED_TYPE;
    gWorldGuide.sch.blocks = gWorldGuide.sch.data = nullptr;
    gWorldGuide.nbtVersion = 0;

    // Default terrain file: bundle Resources/ first, then exe dir for standalone runs
    wxString terrainPath = wxStandardPaths::Get().GetResourcesDir() + "/terrainExt.png";
    if (!wxFileExists(terrainPath)) {
        wxFileName exeFN(wxStandardPaths::Get().GetExecutablePath());
        terrainPath = exeFN.GetPath() + "/terrainExt.png";
    }

    // Default world saves path
    wxString savesDir = wxGetHomeDir() + "/Library/Application Support/minecraft/saves";

    // Restore persisted settings
    wxConfig cfg("Mineways");
    wxString cfgTerrain, cfgSaves, cfgExportPath;
    if (cfg.Read("terrainPath", &cfgTerrain) && wxFileExists(cfgTerrain))
        terrainPath = cfgTerrain;
    if (cfg.Read("savesDir", &cfgSaves))
        savesDir = cfgSaves;
    if (cfg.Read("exportPath", &cfgExportPath))
        mbstowcs(gExportSettings.outputPath, cfgExportPath.utf8_str(), 4096);
    gCurX = cfg.ReadDouble("curX", 0.0);
    gCurZ = cfg.ReadDouble("curZ", 0.0);
    gCurScale = cfg.ReadDouble("curScale", 1.0);
    if (gCurScale < 0.01) gCurScale = 1.0;

    mbstowcs(gSelectTerrainPathAndName, terrainPath.utf8_str(), MAX_PATH_AND_FILE);
    splitPath(gSelectTerrainPathAndName, gSelectTerrainDir, nullptr);
    mbstowcs(gWorldPathDefault, savesDir.utf8_str(), MAX_PATH_AND_FILE);

    // Initialize block colors
    SetMapPremultipliedColors(0);

    // Standard culling scheme: seeds barrier/structure-void as culled by default
    applyCullingScheme(nullptr);

    // Build UI
    BuildMenu();
    CreateStatusBar(2);
    SetStatusText("No world loaded", 0);

    // Top panel with two slider rows
    wxPanel* topPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 60));
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

    topSizer->Add(new wxStaticText(topPanel, wxID_ANY, "Depth:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    m_sliderTop = new wxSlider(topPanel, ID_SLIDER_TOP, INIT_MAP_MAX_HEIGHT, 0, INIT_MAP_MAX_HEIGHT,
                               wxDefaultPosition, wxSize(200, -1), wxSL_HORIZONTAL);
    m_labelTop = new wxStaticText(topPanel, wxID_ANY, wxString::Format("%d", INIT_MAP_MAX_HEIGHT),
                                  wxDefaultPosition, wxSize(40, -1));
    topSizer->Add(m_sliderTop, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    topSizer->Add(m_labelTop,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    topSizer->AddSpacer(20);
    topSizer->Add(new wxStaticText(topPanel, wxID_ANY, "Bottom:"), 0, wxALIGN_CENTER_VERTICAL);
    m_sliderBot = new wxSlider(topPanel, ID_SLIDER_BOT, 0, 0, INIT_MAP_MAX_HEIGHT,
                               wxDefaultPosition, wxSize(200, -1), wxSL_HORIZONTAL);
    m_labelBot = new wxStaticText(topPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(40, -1));
    topSizer->Add(m_sliderBot, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    topSizer->Add(m_labelBot,  0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

    topPanel->SetSizer(topSizer);

    // Map panel
    m_mapPanel = new MapPanel(this);
    m_mapPanel->SetFocus();

    // Frame sizer
    wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);
    frameSizer->Add(topPanel, 0, wxEXPAND);
    frameSizer->Add(m_mapPanel, 1, wxEXPAND);
    SetSizer(frameSizer);
    Layout();
}

MinewaysFrame::~MinewaysFrame()
{
    // Persist settings to plist via wxConfig
    wxConfig cfg("Mineways");
    char buf[4096];
    wcstombs(buf, gSelectTerrainPathAndName, sizeof(buf));
    cfg.Write("terrainPath",  wxString::FromUTF8(buf));
    char savesPath[4096]; wcstombs(savesPath, gWorldPathDefault, sizeof(savesPath));
    cfg.Write("savesDir",    wxString::FromUTF8(savesPath));
    if (gExportSettings.outputPath[0]) {
        wcstombs(buf, gExportSettings.outputPath, sizeof(buf));
        cfg.Write("exportPath", wxString::FromUTF8(buf));
    }
    cfg.Write("curX",     gCurX);
    cfg.Write("curZ",     gCurZ);
    cfg.Write("curScale", gCurScale);

    gFrame = nullptr;
    free(gMapBits);
    gMapBits = nullptr;
}

// Scan the saves directory and populate gWorldDirs[]/gWorldDisplayNames[].
// Mirrors Win/Mineways.cpp's loadWorldList: validates each candidate via
// GetFileVersion (skips unreadable/unsupported saves, e.g. Bedrock folders
// that happen to contain a level.dat) and prefers the level's in-game name
// over the raw folder name.
// Returns the number of worlds found.
static int ScanWorldSaves(const wxString& savesDir)
{
    gNumWorlds = 0;
    char path[4096];
    strncpy(path, savesDir.utf8_str(), sizeof(path) - 1); path[sizeof(path)-1] = '\0';
    DIR* d = opendir(path);
    if (!d) return 0;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr && gNumWorlds < MAX_WORLDS) {
        if (de->d_name[0] == '.') continue;         // skip . / .. / hidden
        wxString entry = savesDir + "/" + de->d_name;
        // A valid world directory contains level.dat
        if (!wxFileExists(entry + "/level.dat")) continue;

        wchar_t wpath[MAX_PATH_AND_FILE], fileOpened[MAX_PATH_AND_FILE];
        mbstowcs(wpath, entry.utf8_str(), MAX_PATH_AND_FILE);
        int version = 0;
        if (GetFileVersion(wpath, &version, fileOpened, MAX_PATH_AND_FILE) != 0)
            continue;   // unreadable/unsupported (e.g. Bedrock) — skip like Windows does

        char levelName[MAX_PATH_AND_FILE] = {};
        wxString display = (GetLevelName(wpath, levelName, MAX_PATH_AND_FILE) == 0 && levelName[0])
            ? wxString::FromUTF8(levelName) : wxFileName(entry).GetFullName();

        gWorldDirs[gNumWorlds] = entry;
        gWorldDisplayNames[gNumWorlds] = display;
        gNumWorlds++;
    }
    closedir(d);
    return gNumWorlds;
}

void MinewaysFrame::BuildMenu()
{
    // Scan worlds before building menu so world submenu is populated
    char defPath[4096]; wcstombs(defPath, gWorldPathDefault, sizeof(defPath));
    ScanWorldSaves(wxString::FromUTF8(defPath));

    wxMenuBar* mb = new wxMenuBar;

    wxMenu* fileMenu = new wxMenu;

    // World list submenu
    wxMenu* worldsMenu = new wxMenu;
    worldsMenu->Append(ID_TEST_BLOCK_WORLD, "Test Block World",
                       "Built-in test world showing all block types");
    if (gNumWorlds > 0) {
        worldsMenu->AppendSeparator();
        for (int i = 0; i < gNumWorlds; i++) {
            worldsMenu->Append(ID_WORLD_ITEM_BASE + i, gWorldDisplayNames[i]);
        }
    }
    fileMenu->AppendSubMenu(worldsMenu, "Open World");

    fileMenu->Append(ID_OPEN_FILE,  "Open level.dat or Schematic...\tCtrl+O",
                     "Open a level.dat or .schematic/.schem file");
    fileMenu->Append(ID_GO_TO_LOCATION, "Go To Location...\tCtrl+G",
                     "Jump map view to a specific X,Z coordinate");
    fileMenu->Append(ID_CHOOSE_TERRAIN, "Choose Terrain File...",
                     "Select a custom terrainExt*.png texture atlas");
    fileMenu->Append(ID_CULLING_SCHEMES, "Culling Schemes...",
                     "Manage block culling schemes (hide blocks from map and export)");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_EXPORT_OBJ, "Export Model...\tCtrl+E",
                     "Export selected region to OBJ");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "Quit\tCtrl+Q");

    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "About Mineways");

    mb->Append(fileMenu, "&File");
    mb->Append(helpMenu, "&Help");
    SetMenuBar(mb);
}

void MinewaysFrame::OnMapPanelSize(int w, int h)
{
    gMapWidth  = w;
    gMapHeight = h;
    free(gMapBits);
    gMapBits = (unsigned char*)malloc((size_t)w * h * 4);
    if (gMapBits) memset(gMapBits, 0xff, (size_t)w * h * 4);
}

void MinewaysFrame::UpdateBottomSlider(int depth)
{
    if (m_sliderBot) {
        m_sliderBot->SetValue(depth);
        m_labelBot->SetLabel(wxString::Format("%d", depth));
    }
}

void MinewaysFrame::UpdateStatusBar(int mx, int mz, int my,
                                    const char* label, int /*type*/, int /*dataVal*/, int /*biome*/)
{
    if (label && *label) {
        wxString s = wxString::Format("X=%d Y=%d Z=%d  %s", mx, my, mz, label);
        SetStatusText(s, 0);
    }
}

// ─── Menu handlers ────────────────────────────────────────────────────────────
void MinewaysFrame::OnOpenWorld(wxCommandEvent&)
{
    // Fallback: manual directory browse (used if the saves scan found nothing)
    char defPath[4096] = "";
    wcstombs(defPath, gWorldPathDefault, sizeof(defPath));
    wxDirDialog dlg(this, "Select a Minecraft world folder",
                    wxString::FromUTF8(defPath),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    LoadWorldFromDir(dlg.GetPath());
}

void MinewaysFrame::OnTestBlockWorld(wxCommandEvent&)
{
    gWorldGuide.type  = WORLD_TEST_BLOCK_TYPE;
    gWorldGuide.world[0] = 0;
    gVersionID        = 3953;   // current data version
    gMinecraftVersion = DATA_VERSION_TO_RELEASE_NUMBER(gVersionID);
    gMinHeight        = ZERO_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    gMaxHeight        = MAX_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    // block_alloc uses gWorldGuide.minHeight/maxHeight to size the grid allocation
    gWorldGuide.minHeight = gMinHeight;
    gWorldGuide.maxHeight = gMaxHeight;
    gCurX = gCurZ     = 0.0;
    gCurScale         = 1.0;
    gCurDepth         = gMaxHeight;
    gTargetDepth      = gMinHeight;
    gLoaded           = TRUE;

    SetTitle("Mineways — Test Block World");
    if (m_sliderTop) {
        m_sliderTop->SetRange(gMinHeight, gMaxHeight);
        m_sliderTop->SetValue(gCurDepth);
        m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
        m_sliderBot->SetRange(gMinHeight, gMaxHeight);
        m_sliderBot->SetValue(gTargetDepth);
        m_labelBot->SetLabel(wxString::Format("%d", gTargetDepth));
    }
    SetStatusText("Test Block World loaded", 0);
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnGoToLocation(wxCommandEvent&)
{
    setLocationData((int)gCurX, (int)gCurZ);
    if (doLocation(this)) {
        int x, z;
        getLocationData(x, z);
        gCurX = x; gCurZ = z;
        if (m_mapPanel) m_mapPanel->RedrawMap();
    }
}

void MinewaysFrame::OnWorldMenuItem(wxCommandEvent& e)
{
    int idx = e.GetId() - ID_WORLD_ITEM_BASE;
    if (idx < 0 || idx >= gNumWorlds) return;
    LoadWorldFromDir(gWorldDirs[idx]);
}

void MinewaysFrame::OnCullingSchemes(wxCommandEvent&)
{
    doCullingSchemesMac(this);
    if (m_mapPanel && gLoaded) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnChooseTerrainFile(wxCommandEvent&)
{
    char curPath[MAX_PATH_AND_FILE];
    wcstombs(curPath, gSelectTerrainPathAndName, sizeof(curPath));
    wxString defaultDir  = wxFileName(wxString::FromUTF8(curPath)).GetPath();
    wxString defaultFile = wxFileName(wxString::FromUTF8(curPath)).GetFullName();

    wxFileDialog dlg(this, "Choose Terrain File", defaultDir, defaultFile,
                     "Terrain files (terrainExt*.png)|terrainExt*.png|PNG files (*.png)|*.png",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;

    wxString path = dlg.GetPath();
    mbstowcs(gSelectTerrainPathAndName, path.utf8_str(), MAX_PATH_AND_FILE);
    splitPath(gSelectTerrainPathAndName, gSelectTerrainDir, nullptr);
}

void MinewaysFrame::OnOpenFile(wxCommandEvent&)
{
    wxFileDialog dlg(this, "Open world file or schematic", "", "",
                     "Minecraft files (level.dat;*.schematic;*.schem)|level.dat;*.schematic;*.schem|All files (*)|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    wxString path = dlg.GetPath();
    // If the user picked level.dat, open its parent directory as the world
    if (path.EndsWith("level.dat")) {
        LoadWorldFromDir(wxFileName(path).GetPath());
        return;
    }

    // Schematic import
    bool isSponge = path.Lower().EndsWith(".schem");
    bool isLegacy = path.Lower().EndsWith(".schematic");
    if (!isSponge && !isLegacy) {
        wxMessageBox("Unrecognised file type. Open level.dat, .schematic, or .schem files.",
                     "Open error", wxOK | wxICON_WARNING, this);
        return;
    }

    CloseAll();
    free(gWorldGuide.sch.blocks); gWorldGuide.sch.blocks = nullptr;
    free(gWorldGuide.sch.data);   gWorldGuide.sch.data   = nullptr;

    gWorldGuide.type = WORLD_SCHEMATIC_TYPE;
    gWorldGuide.sch.isSponge = isSponge;
    gWorldGuide.sch.repeat   = (path.Find("repeat") != wxNOT_FOUND);
    mbstowcs(gWorldGuide.world, path.utf8_str(), MAX_PATH_AND_FILE);

    int err = LoadSchematicFile(gWorldGuide.world, isSponge);
    if (err != 0) {
        gWorldGuide.type = WORLD_UNLOADED_TYPE;
        wxMessageBox(wxString::Format("Could not load schematic (error %d).", err),
                     "Load error", wxOK | wxICON_ERROR, this);
        return;
    }

    gCurX = gWorldGuide.sch.width  / 2.0;
    gCurZ = gWorldGuide.sch.length / 2.0;
    gCurScale  = 1.0;
    gCurDepth  = gMaxHeight;
    gTargetDepth = gMinHeight;
    gLoaded = TRUE;

    if (m_sliderTop) {
        m_sliderTop->SetRange(gMinHeight, gMaxHeight);
        m_sliderTop->SetValue(gCurDepth);
        m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
    }
    if (m_sliderBot) {
        m_sliderBot->SetRange(gMinHeight, gMaxHeight);
        m_sliderBot->SetValue(gTargetDepth);
        m_labelBot->SetLabel(wxString::Format("%d", gTargetDepth));
    }

    wxString name = wxFileName(path).GetName();
    SetTitle(wxString::Format("Mineways — %s", name));
    SetStatusText(wxString::Format("Schematic: %s  %dx%dx%d",
                                   name, gWorldGuide.sch.width,
                                   gWorldGuide.sch.height, gWorldGuide.sch.length), 0);
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnExportOBJ(wxCommandEvent&)
{
    if (!gLoaded) {
        wxMessageBox("Load a world first.", "No world loaded", wxOK | wxICON_WARNING, this);
        return;
    }

    // Get current selection bounds
    int on = 0;
    int minx = 0, miny = 0, minz = 0, maxx = 0, maxy = 0, maxz = 0;
    GetHighlightState(&on, &minx, &miny, &minz, &maxx, &maxy, &maxz, gMinHeight);
    if (!on) {
        wxMessageBox("Please right-click drag on the map to select a region first.",
                     "No selection", wxOK | wxICON_WARNING, this);
        return;
    }

    // Pre-fill bounds into persistent settings
    gExportSettings.minx = minx; gExportSettings.miny = miny; gExportSettings.minz = minz;
    gExportSettings.maxx = maxx; gExportSettings.maxy = maxy; gExportSettings.maxz = maxz;

    if (doExportDialog(this, gExportSettings, minx, miny, minz, maxx, maxy, maxz) != wxID_OK)
        return;

    // Build ExportFileData from dialog settings
    static ExportFileData efd = {};
    efd.fileType = gExportSettings.fileType;
    efd.minxVal = gExportSettings.minx; efd.minyVal = gExportSettings.miny; efd.minzVal = gExportSettings.minz;
    efd.maxxVal = gExportSettings.maxx; efd.maxyVal = gExportSettings.maxy; efd.maxzVal = gExportSettings.maxz;
    efd.chkCenterModel = gExportSettings.centerModel ? 1 : 0;
    for (int t = 0; t < FILE_TYPE_TOTAL; t++) efd.chkMakeZUp[t] = gExportSettings.zUp ? 1 : 0;
    // material radio buttons
    for (int t = 0; t < FILE_TYPE_TOTAL; t++) {
        efd.radioExportNoMaterials[t]   = (gExportSettings.materialType == 0) ? 1 : 0;
        efd.radioExportMtlColors[t]     = (gExportSettings.materialType == 1) ? 1 : 0;
        efd.radioExportSolidTexture[t]  = 0;
        efd.radioExportFullTexture[t]   = (gExportSettings.materialType == 2) ? 1 : 0;
        efd.radioExportTileTextures[t]  = 0;
    }
    efd.blockSizeVal[efd.fileType] = 1.0f;
    efd.chkCreateModelFiles[efd.fileType] = 1;  // keep files (no ZIP for now)
    efd.chkCreateZip[efd.fileType] = 0;
    efd.chkBlockFacesAtBorders = 1;
    efd.chkBiome = 0;
    efd.tileDirString[0] = '\0';
    efd.chkTextureRGB = 1; efd.chkTextureA = 1; efd.chkTextureRGBA = 1;
    efd.radioScaleByBlock = 1;
    efd.radioRotate0 = 1;

    // Set export flags on gOptions
    gOptions.pEFD = &efd;
    gOptions.exportFlags = 0;
    gOptions.saveFilterFlags = BLF_WHOLE | BLF_ALMOST_WHOLE | BLF_STAIRS | BLF_HALF |
        BLF_MIDDLER | BLF_BILLBOARD | BLF_PANE | BLF_FLATTEN | BLF_FLATTEN_SMALL;

    if (gExportSettings.materialType == 1)
        gOptions.exportFlags |= EXPT_OUTPUT_MATERIALS | EXPT_OUTPUT_OBJ_SEPARATE_TYPES |
                                EXPT_OUTPUT_OBJ_MATERIAL_PER_BLOCK | EXPT_OUTPUT_OBJ_MTL_PER_TYPE;
    else if (gExportSettings.materialType == 2)
        gOptions.exportFlags |= EXPT_OUTPUT_MATERIALS | EXPT_OUTPUT_TEXTURE_IMAGES |
                                EXPT_OUTPUT_OBJ_MTL_PER_TYPE;

    // center model and z-up are handled via pEFD fields, not exportFlags

    // Exe/bundle directory for texture lookup
    wchar_t curDir[MAX_PATH_AND_FILE];
    wxFileName exeFN(wxStandardPaths::Get().GetExecutablePath());
    mbstowcs(curDir, exeFN.GetPath().utf8_str(), MAX_PATH_AND_FILE);

    // Biome/group state
    static int userSelectedBiome = -1, biomeIndex = -1;
    static int groupCount = 0, groupCountSize = 10, groupCountArray[10] = {};

    // Quick I/O probe: can we create a file in the chosen directory at all?
    {
        char probePath[MAX_PATH_AND_FILE];
        wcstombs(probePath, gExportSettings.outputPath, sizeof(probePath));
        FILE* probe = fopen(probePath, "wb");
        if (!probe) {
            wxMessageBox(wxString::Format(
                "Cannot create file — check the path and permissions.\nPath: %s\nerrno: %d (%s)",
                probePath, errno, strerror(errno)),
                "Export pre-check failed", wxOK | wxICON_ERROR, this);
            return;
        }
        fclose(probe);
        remove(probePath);  // clean up test file
    }

    FileList outputFileList;
    outputFileList.count = 0;
    for (int i = 0; i < MAX_OUTPUT_FILES; i++) outputFileList.name[i] = nullptr;

    int errCode = SaveVolume(gExportSettings.outputPath, efd.fileType,
        &gOptions, &gWorldGuide, curDir,
        efd.minxVal, efd.minyVal, efd.minzVal,
        efd.maxxVal, efd.maxyVal, efd.maxzVal,
        gMinHeight, gMaxHeight,
        nullptr /*progress*/, gSelectTerrainPathAndName, (wchar_t*)getSelectedCullingSchemeW(),
        &outputFileList,
        13, 0, gVersionID,
        nullptr /*changeBlock*/, 16 /*instanceChunkSize*/,
        userSelectedBiome, biomeIndex,
        groupCount, groupCountSize, groupCountArray);

    if (errCode >= MW_BEGIN_NOTHING_TO_DO && errCode < MW_BEGIN_ERRORS) {
        // Codes in this range (e.g. MW_NO_BLOCKS_FOUND, MW_ALL_BLOCKS_DELETED) mean
        // SaveVolume deliberately wrote nothing — not an error, but not success either.
        // Mirrors Win/Mineways.cpp's `errCode < MW_BEGIN_NOTHING_TO_DO` success gate.
        wxMessageBox("No file was written: nothing exportable was found in the selected region.",
                     "Nothing exported", wxOK | wxICON_WARNING, this);
    } else if (errCode == MW_NO_ERROR || errCode < MW_BEGIN_ERRORS) {
        // Build message before freeing the list
        wxString msg = "Export complete.";
        if (outputFileList.count > 0) {
            for (int i = 0; i < outputFileList.count; i++) {
                if (outputFileList.name[i]) {
                    char outPath[MAX_PATH_AND_FILE];
                    wcstombs(outPath, outputFileList.name[i], sizeof(outPath));
                    msg += "\n" + wxString::FromUTF8(outPath);
                }
            }
        } else {
            char outPath[MAX_PATH_AND_FILE] = {};
            wcstombs(outPath, gExportSettings.outputPath, sizeof(outPath));
            msg += "\n(expected: " + wxString::FromUTF8(outPath) + ")";
        }
        wxMessageBox(msg, "Export done", wxOK | wxICON_INFORMATION, this);
    } else {
        // Show path even on failure so user can diagnose
        char attemptedPath[MAX_PATH_AND_FILE] = {};
        wcstombs(attemptedPath, gExportSettings.outputPath, sizeof(attemptedPath));
        wxMessageBox(wxString::Format("Export failed (error %d).\nAttempted path: %s",
                     errCode, attemptedPath),
                     "Export error", wxOK | wxICON_ERROR, this);
    }

    // Free file list
    for (int i = 0; i < outputFileList.count; i++) free(outputFileList.name[i]);
}

void MinewaysFrame::OnQuit(wxCommandEvent&) { Close(true); }

void MinewaysFrame::OnAbout(wxCommandEvent&)
{
    wxMessageBox("Mineways " MINEWAYS_VERSION_STRING "\n\n"
                 "By Eric Haines and contributors.\n"
                 "wxWidgets macOS build.",
                 "About Mineways", wxOK | wxICON_INFORMATION, this);
}

// Dragging either depth slider must update the Y bound of an already-drawn
// selection, not just future ones — otherwise the selection used by Export
// silently keeps whatever Y bound was in effect when the box was drawn,
// making the sliders look like they "do nothing". Mirrors Win/Mineways.cpp's
// slider handlers, which re-call SetHighlightState with the existing X/Z
// bounds whenever gCurDepth/gTargetDepth changes.
static void ReapplyHighlightDepth()
{
    if (!gHighlightOn) return;
    int on, minx, miny, minz, maxx, maxy, maxz;
    GetHighlightState(&on, &minx, &miny, &minz, &maxx, &maxy, &maxz, gMinHeight);
    SetHighlightState(on, minx, gTargetDepth, minz, maxx, gCurDepth, maxz,
                      gMinHeight, gMaxHeight, HIGHLIGHT_UNDO_IGNORE);
}

void MinewaysFrame::OnSliderTop(wxCommandEvent&)
{
    gCurDepth = m_sliderTop->GetValue();
    m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
    ReapplyHighlightDepth();
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnSliderBot(wxCommandEvent&)
{
    gTargetDepth = m_sliderBot->GetValue();
    m_labelBot->SetLabel(wxString::Format("%d", gTargetDepth));
    ReapplyHighlightDepth();
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

// ─── World loading ────────────────────────────────────────────────────────────
void MinewaysFrame::LoadWorldFromDir(const wxString& dir)
{
    wchar_t wdir[MAX_PATH_AND_FILE];
    mbstowcs(wdir, dir.utf8_str(), MAX_PATH_AND_FILE);

    // Detect and set the world type
    gWorldGuide.type = WORLD_LEVEL_TYPE;
    wcscpy(gWorldGuide.world, wdir);
    wcscpy(gWorldGuide.directory, wdir);
    gWorldGuide.nbtVersion = 0;
    gWorldGuide.isServerWorld = false;

    // Verify it's a valid Anvil world
    wchar_t fileOpened[MAX_PATH_AND_FILE] = {};
    int nbtVer = 0;
    if (GetFileVersion(wdir, &nbtVer, fileOpened, MAX_PATH_AND_FILE) != 0) {
        wxMessageBox("Could not read level.dat in this folder.", "Load error",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    gWorldGuide.nbtVersion = nbtVer;

    // Spawn/player position from level.dat
    int spawnX = 0, spawnY = 64, spawnZ = 0;
    int playerX = 0, playerY = 64, playerZ = 0;
    int dimension = 0;
    GetSpawn(wdir, &spawnX, &spawnY, &spawnZ);
    if (GetPlayer(wdir, &playerX, &playerY, &playerZ, &dimension) != 0) {
        playerX = spawnX; playerY = spawnY; playerZ = spawnZ;
        gWorldGuide.isServerWorld = true;
    }

    // Version detect
    GetFileVersionId(wdir, &gVersionID);
    gMinecraftVersion = DATA_VERSION_TO_RELEASE_NUMBER(gVersionID);
    gMinHeight = ZERO_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    gMaxHeight = MAX_WORLD_HEIGHT(gVersionID, gMinecraftVersion);
    gWorldGuide.minHeight = gMinHeight;
    gWorldGuide.maxHeight = gMaxHeight;

    // Center view on spawn
    gCurX = (double)spawnX;
    gCurZ = (double)spawnZ;
    gCurDepth = gMaxHeight;
    gCurScale = 1.0;

    gLoaded = TRUE;

    SetTitle(wxString::Format("Mineways - %s", dir.AfterLast('/')));
    if (m_sliderTop) {
        m_sliderTop->SetRange(gMinHeight, gMaxHeight);
        m_sliderTop->SetValue(gCurDepth);
        m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
        m_sliderBot->SetRange(gMinHeight, gMaxHeight);
        m_sliderBot->SetValue(gMinHeight);
        m_labelBot->SetLabel(wxString::Format("%d", gMinHeight));
    }
    SetStatusText(wxString::Format("Loaded: %s  (spawn %d,%d,%d)",
                  dir.AfterLast('/'), spawnX, spawnY, spawnZ), 0);

    if (m_mapPanel) m_mapPanel->RedrawMap();
}
