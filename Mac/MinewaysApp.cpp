// MinewaysApp.cpp — wxApp entry point for the macOS / wxWidgets build
#include <wx/wx.h>
#include "MinewaysFrame.h"

class MinewaysApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;
        wxInitAllImageHandlers();
        auto* frame = new MinewaysFrame(nullptr);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(MinewaysApp);
