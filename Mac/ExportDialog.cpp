// ExportDialog.cpp — minimal export dialog for the wxWidgets Mac build
#include <wx/wx.h>
#include <wx/filedlg.h>
#include "ExportDialog.h"
#include "stdafx.h"  // for FILE_TYPE_* constants via blockInfo.h

// File type names and their default extensions
static const char* kTypeNames[] = {
    "Wavefront OBJ (absolute)",
    "Wavefront OBJ (relative)",
    "USD",
    "Binary STL (Magics)",
    "Binary STL (VisCAM)",
    "ASCII STL",
    "VRML2",
    "Schematic (.schematic)",
    "Sponge Schematic (.schem)",
};
static const char* kTypeExts[] = {
    ".obj", ".obj", ".usda", ".stl", ".stl", ".stl", ".wrl", ".schematic", ".schem",
};
static_assert(sizeof(kTypeNames)/sizeof(kTypeNames[0]) == FILE_TYPE_TOTAL, "type count mismatch");

int doExportDialog(wxWindow* parent, ExportSettings& out,
                   int selMinX, int selMinY, int selMinZ,
                   int selMaxX, int selMaxY, int selMaxZ)
{
    wxDialog dlg(parent, wxID_ANY, "Export Model",
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);

    // ── File type ────────────────────────────────────────────────────────────
    wxArrayString typeChoices;
    for (auto& n : kTypeNames) typeChoices.Add(n);
    auto* typeChoice = new wxChoice(&dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, typeChoices);
    typeChoice->SetSelection(out.fileType < FILE_TYPE_TOTAL ? out.fileType : 0);

    wxFlexGridSizer* typeRow = new wxFlexGridSizer(1, 2, 4, 8);
    typeRow->AddGrowableCol(1);
    typeRow->Add(new wxStaticText(&dlg, wxID_ANY, "File type:"), 0, wxALIGN_CENTER_VERTICAL);
    typeRow->Add(typeChoice, 1, wxEXPAND);
    top->Add(typeRow, 0, wxEXPAND | wxALL, 8);

    // ── Output path ──────────────────────────────────────────────────────────
    char initPath[4096] = {};
    if (out.outputPath[0]) wcstombs(initPath, out.outputPath, sizeof(initPath));
    auto* pathCtrl = new wxTextCtrl(&dlg, wxID_ANY, wxString::FromUTF8(initPath),
                                    wxDefaultPosition, wxSize(360, -1));
    auto* browseBtn = new wxButton(&dlg, wxID_ANY, "Browse...");
    browseBtn->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
        int sel = typeChoice->GetSelection();
        if (sel < 0 || sel >= FILE_TYPE_TOTAL) sel = 0;
        wxString wildcard = wxString::Format("Export files (*%s)|*%s|All files (*)|*",
                                             kTypeExts[sel], kTypeExts[sel]);
        wxFileDialog fd(&dlg, "Save export as", "", "", wildcard,
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (pathCtrl->GetValue().length() > 0)
            fd.SetPath(pathCtrl->GetValue());
        if (fd.ShowModal() == wxID_OK)
            pathCtrl->SetValue(fd.GetPath());
    });

    wxFlexGridSizer* pathRow = new wxFlexGridSizer(1, 3, 4, 6);
    pathRow->AddGrowableCol(1);
    pathRow->Add(new wxStaticText(&dlg, wxID_ANY, "Output:"), 0, wxALIGN_CENTER_VERTICAL);
    pathRow->Add(pathCtrl, 1, wxEXPAND);
    pathRow->Add(browseBtn, 0);
    top->Add(pathRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // ── Bounds ───────────────────────────────────────────────────────────────
    top->Add(new wxStaticText(&dlg, wxID_ANY, "Export bounds (X/Y/Z min–max):"),
             0, wxLEFT | wxRIGHT, 8);
    wxFlexGridSizer* bounds = new wxFlexGridSizer(2, 7, 4, 6);
    for (int c = 1; c <= 6; c++) bounds->AddGrowableCol(c);

    auto makeField = [&](int val) {
        return new wxTextCtrl(&dlg, wxID_ANY, wxString::Format("%d", val),
                              wxDefaultPosition, wxSize(60, -1));
    };
    auto* mnX = makeField(selMinX); auto* mxX = makeField(selMaxX);
    auto* mnY = makeField(selMinY); auto* mxY = makeField(selMaxY);
    auto* mnZ = makeField(selMinZ); auto* mxZ = makeField(selMaxZ);

    bounds->Add(new wxStaticText(&dlg, wxID_ANY, "Min:"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mnX, 1, wxEXPAND); bounds->Add(new wxStaticText(&dlg, wxID_ANY, "/"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mnY, 1, wxEXPAND); bounds->Add(new wxStaticText(&dlg, wxID_ANY, "/"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mnZ, 1, wxEXPAND); bounds->Add(new wxStaticBox(&dlg, wxID_ANY, ""), 0);  // spacer
    bounds->Add(new wxStaticText(&dlg, wxID_ANY, "Max:"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mxX, 1, wxEXPAND); bounds->Add(new wxStaticText(&dlg, wxID_ANY, "/"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mxY, 1, wxEXPAND); bounds->Add(new wxStaticText(&dlg, wxID_ANY, "/"), 0, wxALIGN_CENTER_VERTICAL);
    bounds->Add(mxZ, 1, wxEXPAND); bounds->Add(new wxStaticBox(&dlg, wxID_ANY, ""), 0);
    top->Add(bounds, 0, wxEXPAND | wxALL, 8);

    // ── Materials ────────────────────────────────────────────────────────────
    auto* matBox  = new wxStaticBoxSizer(new wxStaticBox(&dlg, wxID_ANY, "Materials"), wxVERTICAL);
    auto* rNone   = new wxRadioButton(&dlg, wxID_ANY, "No materials",    wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    auto* rColors = new wxRadioButton(&dlg, wxID_ANY, "Solid colors");
    auto* rTex    = new wxRadioButton(&dlg, wxID_ANY, "Full textures");
    if (out.materialType == 1) rColors->SetValue(true);
    else if (out.materialType == 2) rTex->SetValue(true);
    else rNone->SetValue(true);
    matBox->Add(rNone,   0, wxTOP | wxLEFT, 4);
    matBox->Add(rColors, 0, wxLEFT, 4);
    matBox->Add(rTex,    0, wxLEFT | wxBOTTOM, 4);
    top->Add(matBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // ── Misc options ─────────────────────────────────────────────────────────
    auto* chkCenter = new wxCheckBox(&dlg, wxID_ANY, "Center model");
    auto* chkZUp    = new wxCheckBox(&dlg, wxID_ANY, "Z is up");
    chkCenter->SetValue(out.centerModel);
    chkZUp->SetValue(out.zUp);
    top->Add(chkCenter, 0, wxLEFT | wxBOTTOM, 8);
    top->Add(chkZUp,    0, wxLEFT | wxBOTTOM, 8);

    // ── Buttons ──────────────────────────────────────────────────────────────
    top->Add(dlg.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);

    dlg.SetSizerAndFit(top);
    dlg.CentreOnParent();

    if (dlg.ShowModal() != wxID_OK) return wxID_CANCEL;

    // Validate output path
    wxString path = pathCtrl->GetValue().Trim();
    if (path.IsEmpty()) {
        wxMessageBox("Please choose an output file path.", "Missing path",
                     wxOK | wxICON_WARNING, &dlg);
        return wxID_CANCEL;
    }

    // Expand leading ~/ (fopen does not expand tilde)
    if (path.StartsWith("~/") || path == "~") {
        const char* home = getenv("HOME");
        if (home)
            path = wxString::FromUTF8(home) + path.Mid(1);
    }

    // Collect results
    out.fileType = typeChoice->GetSelection();

    // Append extension if missing
    const char* ext = kTypeExts[out.fileType];
    if (!path.Lower().EndsWith(ext))
        path += ext;
    mbstowcs(out.outputPath, path.utf8_str(), 4096);

    auto readInt = [](wxTextCtrl* c, int def) -> int {
        long v; return c->GetValue().ToLong(&v) ? (int)v : def;
    };
    out.minx = readInt(mnX, selMinX); out.maxx = readInt(mxX, selMaxX);
    out.miny = readInt(mnY, selMinY); out.maxy = readInt(mxY, selMaxY);
    out.minz = readInt(mnZ, selMinZ); out.maxz = readInt(mxZ, selMaxZ);

    out.materialType = rTex->GetValue() ? 2 : rColors->GetValue() ? 1 : 0;
    out.centerModel  = chkCenter->GetValue();
    out.zUp          = chkZUp->GetValue();

    return wxID_OK;
}
