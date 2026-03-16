#define NOMINMAX

#include <windows.h>
#include <windowsx.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <algorithm>
#include <math.h>
#include <optional>

// Draw content to a DIB, then use UpdateLayeredWindow
// Alpha = 0 (transparent) for background, 255 (opaque) for drawn items

HWND g_hwnd = NULL;
#define RGBA(r, g, b, a) DWORD(((b) | (DWORD(g) << 8)) | ((DWORD(r)) << 16) | ((DWORD(a)) << 24))

#define ARGB(a, r, g, b) ((DWORD(a) << 24) | (DWORD(r) << 16) | (DWORD(g) << 8) | DWORD(b))
#define ARGBf(name, dval) float name##a = float((dval >> 24) & 0xFF), name##r = float((dval >> 16) & 0xFF), name##g = float((dval >> 8) & 0xFF), name##b = float((dval) & 0xFF);

void CompositeOverwrite(DWORD& back, DWORD front)
{
    back = front;
    ARGBf(b, front);
    constexpr float divier = 255.f;
    br *= ba / divier, bg *= ba / divier, bb *= ba / divier;
    back = ARGB(ba, br, bg, bb);
}

void CompositeAlpha(DWORD& back, DWORD front)
{
    ARGBf(f, front);
    ARGBf(b, front);

    if (fa == 255) {
        back = front;
        return;
    }
    if (fa == 0)
        return;

    float inv_af = 255 - fa;
    constexpr float divier = 1.f / 255;
    back = ARGB(fa + (ba * inv_af) * divier,
        (fr * fa + br * inv_af) * divier,
        (fg * fa + bg * inv_af) * divier,
        (fb * fa + bb * inv_af) * divier);
}

DWORD LerpColor(DWORD colorA, DWORD colorB, float t)
{
    t = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (t == 0)
        return colorA;
    if (t == 1)
        return colorB;

    ARGBf(a, colorA);
    ARGBf(b, colorB);
    return ARGB(aa + t * (ba - aa), ar + t * (br - ar), ag + t * (bg - ag), ab + t * (bb - ab));
}

struct Canvas {
    DWORD* pixels;
    LONG w, h;
    std::optional<RECT> customRegion;
    Canvas operator&(RECT rect)
    {
        Canvas result = *this;
        result.customRegion = rect;
        return result;
    }
};

template <void (*Composite)(DWORD& back, DWORD front) = CompositeAlpha>
void drawBorderedRect(const Canvas canvas, const RECT rc, int radius, int bw, DWORD bgCol, DWORD b_col)
{
    DWORD* pixels = canvas.pixels;

    int cl = 0, ct = 0, cr = canvas.w, cb = canvas.h;
    if (canvas.customRegion) {
        cl = std::max(cl, (int)canvas.customRegion->left);
        cr = std::min(cr, (int)canvas.customRegion->right);
        ct = std::max(ct, (int)canvas.customRegion->top);
        cb = std::max(cb, (int)canvas.customRegion->bottom);
    }

    const int rcl = rc.left;
    const int rcr = rc.right;
    const int rct = rc.top;
    const int rcb = rc.bottom;
    const int rcw = rc.right - rc.left;
    const int rch = rc.bottom - rc.top;

    const int minrx = std::min(radius, std::max(rcw / 2, 0));
    const int minrx_right = std::min(radius, std::max((rcw + 1) / 2, 0));

    const int minry = std::min(radius, std::max(rch / 2, 0));
    const int minry_bottom = std::min(radius, std::max((rch + 1) / 2, 0));

    const int bwx = std::min(bw, minrx);
    const int bwy = std::min(bw, minry);
    const int bwy_bottom = std::min(bw, minry_bottom);

    const int rcr_minrx_start = std::max(rcr - minrx, cl);
    const int rcr_minrx_end = std::min(rcr - minrx_right, cr);

    const int rcl_start = std::max(rcl, cl);
    const int rct_start = std::max(rct, ct);

    const int rct_bwy = rct + bwy;
    const int rct_bwy_start = std::max(rct_bwy, ct);
    const int rct_bwy_end = std::min(rct_bwy, cb);

    const int rcr_bwx = rcr - bwx;
    const int rcr_bw_start = std::max(rcr_bwx, cl);
    const int rcr_bw_end = std::min(rcr_bwx, cr);

    const int rcb_bwy = rcb - bwy_bottom;
    const int rcb_bw_start = std::max(rcb_bwy, ct);
    const int rcb_bw_end = std::min(rcb_bwy, cb);

    const int rcb_minry = rcb - minry_bottom;
    const int rcb_minry_start = std::max(rcb_minry, ct);
    const int rcb_minry_end = std::min(rcb_minry, cb);

    const int rct_minry = rct + minry;
    const int rct_minry_start = std::max(rct_minry, ct);
    const int rct_minry_end = std::min(rct_minry, cb);

    const int rcl_bwx = rcl + bwx;
    const int rcl_bwx_start = std::max(rcl_bwx, cl);
    const int rcl_bwx_end = std::min(rcl_bwx, cr);

    const int rcl_minrx_start = std::max(rcl + minrx, cl);
    const int rcl_minrx_end = std::min(rcl + minrx_right, cr);

    const int rcb_end = std::min(rcb, cb);
    const int rcr_end = std::min(rcr, cr);

    for (int y = rct_start; y < rct_bwy_end; y++) // top border
        for (int x = rcl_minrx_start; x < rcr_minrx_end; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct_bwy_start; y < rct_minry_end; y++) // top section
        for (int x = rcl_minrx_start; x < rcr_minrx_end; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rct_minry_start; y < rcb_minry_end; y++) // mid section
        for (int x = rcl_bwx_start; x < rcr_bw_end; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rcb_minry_start; y < rcb_bw_end; y++) // bottom section
        for (int x = rcl_minrx_start; x < rcr_minrx_end; x++)
            Composite(pixels[y * canvas.w + x], bgCol);

    for (int y = rcb_bw_start; y < rcb_end; y++) // bottom border
        for (int x = rcl_minrx_start; x < rcr_minrx_end; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct_minry_start; y < rcb_minry_end; y++) // left border
        for (int x = rcl_start; x < rcl_bwx_end; x++)
            Composite(pixels[y * canvas.w + x], b_col);

    for (int y = rct_minry_start; y < rcb_minry_end; y++) // right border
        for (int x = rcr_bw_start; x < rcr_end; x++)
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
        rgba[0] = uint8_t(float(rgba[0]) * dist),
        rgba[1] = uint8_t(float(rgba[1]) * dist),
        rgba[2] = uint8_t(float(rgba[2]) * dist),
        rgba[3] = uint8_t(float(rgba[3]) * dist);
        return col;
    };

    for (int y = rct_start; y < rct_minry_end; y++) // top left
        for (int x = rcl_start; x < rcl_minrx_end; x++) {
            float dist = makeDist(x - rcl, y - rct, radius);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rct_start; y < rct_minry_end; y++) // top right
        for (int x = rcr_minrx_start; x < rcr_end; x++) {
            float dist = makeDist(rcw - x - 1 + rcl, y - rct, radius);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rcb_minry_start; y < rcb_end; y++) // bottom left
        for (int x = rcl_start; x < rcl_minrx_end; x++) {
            float dist = makeDist(x - rcl, rch - y - 1 + rct, radius);
            Composite(pixels[y * canvas.w + x], makeColor(dist));
        }

    for (int y = rcb_minry_start; y < rcb_end; y++) // bottom right
        for (int x = rcr_minrx_start; x < rcr_end; x++) {
            float dist = makeDist(rcw - x - 1 + rcl, rch - y - 1 + rct, radius);
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

    drawBorderedRect<CompositeOverwrite>(canvas, { 0, 0, width, height }, 16, 3, 0x88333333, 0x88AAAAAA);
    drawBorderedRect(canvas, { 8, 8, 100, height - 8 }, 8, 3, 0xAA6699BB, 0xFF6699BB);
    drawBorderedRect(canvas, { 108, 8, 200, height - 8 }, 8, 3, 0xAABB0044, 0xFFBB0044);
    drawBorderedRect(canvas, { 208, 8, 300, height - 8 }, 8, 3, 0xAAAAAAAA, 0xFFAAAAAA);

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