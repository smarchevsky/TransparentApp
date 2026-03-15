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

DWORD makeRGBA(float fr, float fg, float fb, float fa)
{
    BYTE r = (BYTE)std::clamp(fr * 256.f, 0.f, 255.f);
    BYTE g = (BYTE)std::clamp(fg * 256.f, 0.f, 255.f);
    BYTE b = (BYTE)std::clamp(fb * 256.f, 0.f, 255.f);
    BYTE a = (BYTE)std::clamp(fa * 256.f, 0.f, 255.f);
    return (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | ((b * a / 255));
}

struct Canvas {
    DWORD* pixels;
    LONG w, h;
};

void drawBorderedRect(const Canvas canvas)
{
    const float bgMag = 0.2f;
    const float borderMag = 0.6f;
    const float alpha = 0.8f;
    const int r = 16;
    const int bw = 2;

    const int rcr = canvas.w; // right
    const int rcb = canvas.h; // bottom

    DWORD bgColor = makeRGBA(bgMag, bgMag, bgMag, alpha);
    DWORD borderColor = makeRGBA(borderMag, borderMag, borderMag, alpha);

    DWORD* pixels = canvas.pixels;

    int rcr_r = rcr - r;
    int rcr_bw = rcr - bw;
    int rcb_bw = rcb - bw;
    int rcb_r = rcb - r;

    for (int y = 0; y < bw; y++) // top border
        for (int x = r; x < rcr_r; x++)
            pixels[y * canvas.w + x] = borderColor;

    for (int y = bw; y < r; y++) // top background
        for (int x = r; x < rcr_r; x++)
            pixels[y * canvas.w + x] = bgColor;

    for (int y = r; y < rcb_r; y++) // mid section
        for (int x = bw; x < rcr_bw; x++)
            pixels[y * canvas.w + x] = bgColor;

    for (int y = rcb_r; y < rcb_bw; y++) // bottom background
        for (int x = r; x < rcr_r; x++)
            pixels[y * canvas.w + x] = bgColor;

    for (int y = rcb_bw; y < rcb; y++) // bottom border
        for (int x = r; x < rcr_r; x++)
            pixels[y * canvas.w + x] = borderColor;

    for (int y = r; y < rcb_r; y++) // left border
        for (int x = 0; x < bw; x++)
            pixels[y * canvas.w + x] = borderColor;

    for (int y = r; y < rcb_r; y++) // right border
        for (int x = rcr_bw; x < rcr; x++)
            pixels[y * canvas.w + x] = borderColor;

    auto makeDist = [](int x, int y, int r) {
        float fx = float(r - x), fy = float(r - y);
        return r - sqrtf(fx * fx + fy * fy);
    };

    auto makeColor = [&](float dist) {
        dist += 1;
        float mag = borderMag - std::clamp(dist - bw, 0.f, 1.f) * (borderMag - bgMag);
        return makeRGBA(mag, mag, mag, std::clamp(dist, 0.f, 1.f) * alpha);
    };

    // const float r = std::min(16, std::min(width / 2, height / 2));
    int minrx = std::min(r, (rcr + 1) / 2);
    int minry = std::min(r, (rcb + 1) / 2);

    for (int y = 0; y < minry; y++)
        for (int x = 0; x < minrx; x++) {
            float dist = makeDist(x, y, r);
            pixels[y * rcr + x] = makeColor(dist);
        }

    for (int y = 0; y < minry; y++)
        for (int x = rcr - minrx; x < rcr; x++) {
            float dist = makeDist(rcr - x - 1, y, r);
            pixels[y * rcr + x] = makeColor(dist);
        }

    for (int y = rcb - minry; y < rcb; y++)
        for (int x = 0; x < minrx; x++) {
            float dist = makeDist(x, rcb - y - 1, r);
            pixels[y * rcr + x] = makeColor(dist);
        }

    for (int y = rcb - minry; y < rcb; y++)
        for (int x = rcr - minrx; x < rcr; x++) {
            float dist = makeDist(rcr - x - 1, rcb - y - 1, r);
            pixels[y * rcr + x] = makeColor(dist);
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

    drawBorderedRect(canvas);

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