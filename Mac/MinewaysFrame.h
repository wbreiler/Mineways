#pragma once
#include <wx/wx.h>
#include "MapPanel.h"

class MinewaysFrame : public wxFrame {
public:
    explicit MinewaysFrame(wxWindow* parent);
    ~MinewaysFrame();

    // Called by MapPanel mouse handlers (shared state lives here)
    void UpdateStatusBar(int mx, int mz, int my, const char* label, int type, int dataVal, int biome);
    void OnMapPanelSize(int w, int h);

private:
    MapPanel*    m_mapPanel   = nullptr;
    wxSlider*    m_sliderTop  = nullptr;   // top-depth (ceiling)
    wxSlider*    m_sliderBot  = nullptr;   // bottom-depth (floor)
    wxStaticText* m_labelTop  = nullptr;
    wxStaticText* m_labelBot  = nullptr;

    void BuildMenu();
    void LoadWorldFromDir(const wxString& dir);

    // File menu
    void OnOpenWorld(wxCommandEvent&);
    void OnOpenFile(wxCommandEvent&);
    void OnExportOBJ(wxCommandEvent&);
    void OnQuit(wxCommandEvent&);
    // View menu
    void OnSliderTop(wxCommandEvent&);
    void OnSliderBot(wxCommandEvent&);
    // Help menu
    void OnAbout(wxCommandEvent&);

    wxDECLARE_EVENT_TABLE();
};

// IDs
enum {
    ID_OPEN_WORLD  = wxID_HIGHEST + 1,
    ID_OPEN_FILE,
    ID_EXPORT_OBJ,
    ID_SLIDER_TOP,
    ID_SLIDER_BOT,
};
