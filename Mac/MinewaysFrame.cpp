// MinewaysFrame.cpp — wxWidgets main window, replacing Win32 Mineways.cpp
// Global state mirrors the static variables in Win/Mineways.cpp.
#include <wx/wx.h>
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include "MinewaysFrame.h"
#include "MapPanel.h"

// Win32 shims + project headers (stdafx.h routes to compat.h on non-WIN32)
#include "stdafx.h"

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
static int      gNumWorlds = 0;

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

    // Default terrain file in exe's directory
    wxString exeDir = wxStandardPaths::Get().GetDataDir();
    wxString terrainPath = exeDir + "/terrainExt.png";
    mbstowcs(gSelectTerrainPathAndName, terrainPath.utf8_str(), MAX_PATH_AND_FILE);
    splitPath(gSelectTerrainPathAndName, gSelectTerrainDir, nullptr);

    // Default world saves path: ~/Library/Application Support/minecraft/saves
    wxString savesDir = wxGetHomeDir() + "/Library/Application Support/minecraft/saves";
    mbstowcs(gWorldPathDefault, savesDir.utf8_str(), MAX_PATH_AND_FILE);

    // Initialize block colors
    SetMapPremultipliedColors(0);

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
    gFrame = nullptr;
    free(gMapBits);
    gMapBits = nullptr;
}

// Scan the saves directory and populate gWorldDirs[].
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
        if (wxFileExists(entry + "/level.dat"))
            gWorldDirs[gNumWorlds++] = entry;
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
            wxString name = wxFileName(gWorldDirs[i]).GetFullName();
            worldsMenu->Append(ID_WORLD_ITEM_BASE + i, name);
        }
    }
    fileMenu->AppendSubMenu(worldsMenu, "Open World");

    fileMenu->Append(ID_OPEN_FILE,  "Open level.dat or Schematic...\tCtrl+O",
                     "Open a level.dat or .schematic/.schem file");
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
    gCurX = gCurZ     = 0.0;
    gCurScale         = 1.0;
    gCurDepth         = gMaxHeight;
    gLoaded           = TRUE;

    SetTitle("Mineways — Test Block World");
    if (m_sliderTop) {
        m_sliderTop->SetRange(gMinHeight, gMaxHeight);
        m_sliderTop->SetValue(gCurDepth);
        m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
        m_sliderBot->SetRange(gMinHeight, gMaxHeight);
        m_sliderBot->SetValue(gMinHeight);
        m_labelBot->SetLabel(wxString::Format("%d", gMinHeight));
    }
    SetStatusText("Test Block World loaded", 0);
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnWorldMenuItem(wxCommandEvent& e)
{
    int idx = e.GetId() - ID_WORLD_ITEM_BASE;
    if (idx < 0 || idx >= gNumWorlds) return;
    LoadWorldFromDir(gWorldDirs[idx]);
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
    } else {
        // schematic — placeholder
        wxMessageBox("Schematic import not yet wired up in the wxWidgets build.",
                     "Not implemented", wxOK | wxICON_INFORMATION, this);
    }
}

void MinewaysFrame::OnExportOBJ(wxCommandEvent&)
{
    if (!gLoaded) {
        wxMessageBox("Load a world first.", "No world loaded", wxOK | wxICON_WARNING, this);
        return;
    }
    wxMessageBox("OBJ export UI is not yet wired up in the wxWidgets build.\n"
                 "Use the Windows build or the command-line (headless) mode.",
                 "Not implemented", wxOK | wxICON_INFORMATION, this);
}

void MinewaysFrame::OnQuit(wxCommandEvent&) { Close(true); }

void MinewaysFrame::OnAbout(wxCommandEvent&)
{
    wxMessageBox("Mineways " MINEWAYS_VERSION_STRING "\n\n"
                 "By Eric Haines and contributors.\n"
                 "wxWidgets macOS build.",
                 "About Mineways", wxOK | wxICON_INFORMATION, this);
}

void MinewaysFrame::OnSliderTop(wxCommandEvent&)
{
    gCurDepth = m_sliderTop->GetValue();
    m_labelTop->SetLabel(wxString::Format("%d", gCurDepth));
    if (m_mapPanel) m_mapPanel->RedrawMap();
}

void MinewaysFrame::OnSliderBot(wxCommandEvent&)
{
    gTargetDepth = m_sliderBot->GetValue();
    m_labelBot->SetLabel(wxString::Format("%d", gTargetDepth));
    // bottom depth affects export bounds; map display uses gCurDepth (top slider)
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
