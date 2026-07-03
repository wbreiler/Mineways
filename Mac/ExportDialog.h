#pragma once
#include <wx/wx.h>

// Result collected from the export dialog.
struct ExportSettings {
    int     fileType;       // FILE_TYPE_* constant
    wchar_t outputPath[4096];
    int     minx, miny, minz, maxx, maxy, maxz;
    int     materialType;   // 0=none, 1=colors, 2=textures
    bool    centerModel;
    bool    zUp;
};

// Returns wxID_OK or wxID_CANCEL.
int doExportDialog(wxWindow* parent, ExportSettings& out,
                   int selMinX, int selMinY, int selMinZ,
                   int selMaxX, int selMaxY, int selMaxZ);
