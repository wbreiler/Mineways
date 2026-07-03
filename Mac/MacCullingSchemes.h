#pragma once
#include <wx/wx.h>

// Mac-side entry point for the Culling Scheme manager dialog.
// Called from MinewaysFrame; opens the scheme list and editor.
void doCullingSchemesMac(wxWindow* parent);

// Returns the name of the last scheme selected (or L"Standard").
const wchar_t* getSelectedCullingSchemeW();
