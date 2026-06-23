// MapPanel.cpp — wxPanel that displays the Mineways map
#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include "MapPanel.h"
#include "MinewaysFrame.h"
#include "stdafx.h"   // block info types (for IDBlock)

// Globals owned by MinewaysFrame.cpp
extern double& GetCurX();
extern double& GetCurZ();
extern double& GetCurScale();
extern BOOL    IsLoaded();
extern unsigned char* GetMapBits();
extern int     GetMapWidth();
extern int     GetMapHeight();
extern const char* QueryBlock(int bx, int by, int* mx, int* my, int* mz, int* type, int* dataVal, int* biome);
extern void    RedrawMapIntoBuffer();
extern MinewaysFrame* gFrame;   // ponytail: fine as extern, single instance

wxBEGIN_EVENT_TABLE(MapPanel, wxPanel)
    EVT_PAINT(MapPanel::OnPaint)
    EVT_SIZE(MapPanel::OnSize)
    EVT_LEFT_DOWN(MapPanel::OnLeftDown)
    EVT_LEFT_UP(MapPanel::OnLeftUp)
    EVT_MOTION(MapPanel::OnMouseMove)
    EVT_RIGHT_DOWN(MapPanel::OnRightDown)
    EVT_MOUSEWHEEL(MapPanel::OnMouseWheel)
    EVT_KEY_DOWN(MapPanel::OnKeyDown)
wxEND_EVENT_TABLE()

MapPanel::MapPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

MapPanel::~MapPanel()
{
    free(m_bits);
}

void MapPanel::ResizeBuffer(int w, int h)
{
    if (w == m_w && h == m_h) return;
    m_w = w; m_h = h;
    free(m_bits);
    m_bits = (unsigned char*)malloc((size_t)w * h * 4);
    if (m_bits) memset(m_bits, 0xff, (size_t)w * h * 4);
    // Notify frame so the shared gMapBits buffer is also resized
    extern MinewaysFrame* gFrame;
    if (gFrame) gFrame->OnMapPanelSize(w, h);
}

void MapPanel::RedrawMap()
{
    RedrawMapIntoBuffer();
    Refresh(false);
}

void MapPanel::OnSize(wxSizeEvent& e)
{
    wxSize sz = e.GetSize();
    ResizeBuffer(sz.GetWidth(), sz.GetHeight());
    RedrawMap();
    e.Skip();
}

void MapPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    unsigned char* bits = GetMapBits();
    int w = GetMapWidth(), h = GetMapHeight();
    if (!bits || w <= 0 || h <= 0) {
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        return;
    }
    // wxImage expects packed RGB (no alpha channel; we have RGBA)
    // Copy into a separate RGB buffer.  wxImage::SetData takes ownership of malloc'd memory.
    unsigned char* rgb = (unsigned char*)malloc((size_t)w * h * 3);
    if (!rgb) { dc.Clear(); return; }
    for (int i = 0, j = 0; i < w * h * 4; i += 4, j += 3) {
        rgb[j]   = bits[i];
        rgb[j+1] = bits[i+1];
        rgb[j+2] = bits[i+2];
    }
    wxImage img(w, h, rgb, false /*do not static*/);
    dc.DrawBitmap(wxBitmap(img), 0, 0, false);
}

// ─── Mouse handling ─────────────────────────────────────────────────────────
void MapPanel::OnLeftDown(wxMouseEvent& e)
{
    m_dragging = true;
    m_dragStart = e.GetPosition();
    m_dragStartCX = GetCurX();
    m_dragStartCZ = GetCurZ();
    CaptureMouse();
    SetFocus();
}

void MapPanel::OnLeftUp(wxMouseEvent& e)
{
    if (m_dragging) { m_dragging = false; ReleaseMouse(); }
    e.Skip();
}

void MapPanel::OnMouseMove(wxMouseEvent& e)
{
    if (!m_dragging) { e.Skip(); return; }
    wxPoint pos = e.GetPosition();
    double scale = GetCurScale();
    // 1 pixel = 1/scale blocks
    GetCurX() = m_dragStartCX - (pos.x - m_dragStart.x) / scale;
    GetCurZ() = m_dragStartCZ - (pos.y - m_dragStart.y) / scale;
    RedrawMap();
}

void MapPanel::OnRightDown(wxMouseEvent& e)
{
    // right-click: show block info at cursor
    wxPoint pos = e.GetPosition();
    int mx, my, mz, type, dataVal, biome;
    const char* label = QueryBlock(pos.x, pos.y, &mx, &my, &mz, &type, &dataVal, &biome);
    if (gFrame && label)
        gFrame->UpdateStatusBar(mx, mz, my, label, type, dataVal, biome);
    e.Skip();
}

void MapPanel::OnMouseWheel(wxMouseEvent& e)
{
    double& scale = GetCurScale();
    double factor = (e.GetWheelRotation() > 0) ? 1.1 : (1.0 / 1.1);
    scale = wxMax(1.0, wxMin(40.0, scale * factor));
    RedrawMap();
}

void MapPanel::OnKeyDown(wxKeyEvent& e)
{
    double step = 8.0 / GetCurScale();
    switch (e.GetKeyCode()) {
    case WXK_LEFT:  GetCurX() -= step; break;
    case WXK_RIGHT: GetCurX() += step; break;
    case WXK_UP:    GetCurZ() -= step; break;
    case WXK_DOWN:  GetCurZ() += step; break;
    default: e.Skip(); return;
    }
    RedrawMap();
}
