#pragma once
#include <wx/wx.h>

// wxPanel that renders the Minecraft map via DrawMap() into a pixel buffer,
// then blits it using wxClientDC.
class MapPanel : public wxPanel {
public:
    MapPanel(wxWindow* parent);
    ~MapPanel();

    void RedrawMap();      // call DrawMap() + Refresh()
    void ResizeBuffer(int w, int h);

private:
    unsigned char* m_bits = nullptr;
    int m_w = 0, m_h = 0;

    // drag state
    bool   m_dragging = false;
    double m_dragStartCX = 0, m_dragStartCZ = 0;
    wxPoint m_dragStart{0,0};

    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnLeftDown(wxMouseEvent&);
    void OnLeftUp(wxMouseEvent&);
    void OnMouseMove(wxMouseEvent&);
    void OnRightDown(wxMouseEvent&);
    void OnMouseWheel(wxMouseEvent&);
    void OnKeyDown(wxKeyEvent&);

    wxDECLARE_EVENT_TABLE();
};
