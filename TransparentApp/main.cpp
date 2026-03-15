#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <algorithm>
#include <math.h>

// Draw content to a DIB, then use UpdateLayeredWindow
// Alpha = 0 (transparent) for background, 255 (opaque) for drawn items

HWND g_hwnd = NULL;
#define RGBA(r, g, b, a) ((DWORD)(((b) | ((WORD)(g) << 8)) | (((DWORD)(r)) << 16) | (((DWORD)(a)) << 24)))

void CompositeOverwrite(DWORD& back, DWORD front) { back = front; }
void CompositeAlpha(DWORD& back, DWORD front)
{
    float fa = (front >> 24) & 0xFF, fr = (front >> 16) & 0xFF, fg = (front >> 8) & 0xFF, fb = (front) & 0xFF;
    float ba = (back >> 24) & 0xFF, br = (back >> 16) & 0xFF, bg = (back >> 8) & 0xFF, bb = (back) & 0xFF;

    if (fa == 255) {
        back = front;
        return;
    }
    if (fa == 0)
        return;

    float inv_af = 255 - fa;
    back = (DWORD(fa + (ba * inv_af) / 255.f) << 24) | (DWORD((fr * fa + br * inv_af) / 255.f) << 16)
        | (DWORD((fg * fa + bg * inv_af) / 255.f) << 8) | DWORD((fb * fa + bb * inv_af) / 255.f);
}

DWORD LerpColor(DWORD colorA, DWORD colorB, float t)
{
    t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (t == 0)
        return colorA;
    if (t == 1)
        return colorB;

    float a1 = (colorA >> 24) & 0xff, r1 = (colorA >> 16) & 0xff, g1 = (colorA >> 8) & 0xff, b1 = (colorA) & 0xff;
    float a2 = (colorB >> 24) & 0xff, r2 = (colorB >> 16) & 0xff, g2 = (colorB >> 8) & 0xff, b2 = (colorB) & 0xff;
    return (DWORD(a1 + t * (a2 - a1)) << 24) | (DWORD(r1 + t * (r2 - r1)) << 16)
        | (DWORD(g1 + t * (g2 - g1)) << 8) | DWORD(b1 + t * (b2 - b1));
}

struct Canvas {
    DWORD* pixels;
    LONG w, h;
};

template <void (*Composite)(DWORD& back, DWORD front) = CompositeAlpha>
void drawBorderedRect(const Canvas canvas, const RECT rc, int r, int bw, DWORD bgCol, DWORD b_col)
{
    DWORD* pixels = canvas.pixels;

    const int rcl = rc.left;
    const int rcr = rc.right;
    const int rct = rc.top;
    const int rcw = rc.right - rc.left;
    const int rch = rc.bottom - rc.top;

    const int rcw_r = rcl + rcw - r;
    const int rcw_bw = rcl + rcw - bw;
    const int rch_bw = rct + rch - bw;
    const int rch_r = rct + rch - r;
    const int rct_r = rct + r;
    const int rcl_r = rcl + r;

    for (int y = rct + 0; y < rct + bw; y++) // top border
        for (int x = rcl_r; x < rcw_r; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct + bw; y < rct_r; y++) // top background
        for (int x = rcl_r; x < rcw_r; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rct_r; y < rch_r; y++) // mid section
        for (int x = rcl + bw; x < rcw_bw; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rch_r; y < rch_bw; y++) // bottom background
        for (int x = rcl_r; x < rcw_r; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rch_bw; y < rct + rch; y++) // bottom border
        for (int x = rcl_r; x < rcw_r; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct_r; y < rch_r; y++) // left border
        for (int x = rcl + 0; x < rcl + bw; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct_r; y < rch_r; y++) // right border
        for (int x = rcw_bw; x < rcl + rcw; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    // corners
    auto makeDist = [](int x, int y, int r) {
        float fx = float(r - x), fy = float(r - y);
        return r - sqrtf(fx * fx + fy * fy);
    };

    auto makeColor = [&](float dist) {
        union {
            DWORD col;
            uint8_t rgba[4];
        };
        dist += 1;
        col = LerpColor(b_col, bgCol, dist - bw);
        dist = std::clamp(dist, 0.f, 1.f);
        rgba[0] *= dist, rgba[1] *= dist, rgba[2] *= dist, rgba[3] *= dist;
        return col;
    };

    int minrx = std::min(r, (rcw + 1) / 2);
    int minry = std::min(r, (rch + 1) / 2);

    for (int y = rct + 0; y < rct + minry; y++) // top left
        for (int x = rcl; x < rcl + minrx; x++) {
            float dist = makeDist(x - rcl, y - rct, r);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rct; y < rct + minry; y++) // top right
        for (int x = rcl + rcw - minrx; x < rcl + rcw; x++) {
            float dist = makeDist(rcw - x - 1 + rcl, y - rct, r);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rct + rch - minry; y < rct + rch; y++)
        for (int x = rcl + 0; x < rcl + minrx; x++) {
            float dist = makeDist(x - rcl, rch - y - 1 + rct, r);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rct + rch - minry; y < rct + rch; y++)
        for (int x = rcl + rcw - minrx; x < rcl + rcw; x++) {
            float dist = makeDist(rcw - x - 1 + rcl, rch - y - 1 + rct, r);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }
}

void UpdateWindow(HWND hwnd, int width, int height)
{
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Create a 32-bit DIB (ARGB)
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(bih);
    bih.biWidth = width;
    bih.biHeight = -height; // top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    BITMAPINFO bi = {};
    bi.bmiHeader = bih;

    void* pvBits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbm);

    Canvas canvas { (DWORD*)pvBits, width, height };

    drawBorderedRect<CompositeOverwrite>(canvas, { 0, 0, width, height }, 16, 3, 0x88333333, 0x88FFFFFF);
    drawBorderedRect(canvas, { 8, 8, 400, height - 8 }, 8, 3, 0x88FF0000, 0xFFFF0000);

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    RECT wndRect;
    GetWindowRect(hwnd, &wndRect);
    POINT ptDst = { wndRect.left, wndRect.top };
    POINT ptSrc = { 0, 0 };
    SIZE szWnd = { width, height };

    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &szWnd,
        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    // Cleanup
    SelectObject(hdcMem, hOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        if (w > 0 && h > 0)
            UpdateWindow(hwnd, w, h);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize = { 150, 100 };
        return 0;
    }

    case WM_NCCALCSIZE:
        return 0; // client area = entire window, no NC insets

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc;
        GetWindowRect(hwnd, &rc);
        const int B = 8; // border thickness in px

        bool left = pt.x < rc.left + B;
        bool right = pt.x > rc.right - B;
        bool top = pt.y < rc.top + B;
        bool bottom = pt.y > rc.bottom - B;

        if (top && left)
            return HTTOPLEFT;
        if (top && right)
            return HTTOPRIGHT;
        if (bottom && left)
            return HTBOTTOMLEFT;
        if (bottom && right)
            return HTBOTTOMRIGHT;
        if (top)
            return HTTOP;
        if (bottom)
            return HTBOTTOM;
        if (left)
            return HTLEFT;
        if (right)
            return HTRIGHT;

        return HTCLIENT;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int B = 8;
        // only drag if click is away from resize borders
        if (pt.x > B && pt.x < rc.right - B && pt.y > B && pt.y < rc.bottom - B) {
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"LayeredWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST, // layered + always on top
        L"LayeredWnd", L"",
        WS_POPUP, // no title bar/border
        300, 200, 400, 300,
        NULL, NULL, hInst, NULL);

    MARGINS m = { 0 };
    DwmExtendFrameIntoClientArea(hwnd, &m);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}