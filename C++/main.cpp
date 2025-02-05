// overlay_window.cpp
#include <windows.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>

#pragma comment (lib, "gdiplus.lib")
#pragma comment (lib, "Gdi32.lib")
#pragma comment (lib, "User32.lib")

using namespace Gdiplus;

const double SCALE_FACTOR = 0.65;
const int Y_OFFSET = -38;

ULONG_PTR g_GdiToken = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
Bitmap* LoadAndProcessImage(const WCHAR* filename);
Bitmap* ResizeImage(Bitmap* image, double scaleFactor);
void UpdateLayeredWindowBitmap(HWND hWnd, Bitmap* image, int x, int y);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_GdiToken, &gdiplusStartupInput, NULL) != Ok)
    {
        MessageBox(NULL, L"Failed to initialize GDI+.", L"Error", MB_ICONERROR);
        return -1;
    }

    const WCHAR* imagePath = L"mrwashee.png";
    Bitmap* image = LoadAndProcessImage(imagePath);
    if (!image)
    {
        MessageBox(NULL, L"Failed to load the image.", L"Error", MB_ICONERROR);
        GdiplusShutdown(g_GdiToken);
        return -1;
    }

    Bitmap* resizedImage = ResizeImage(image, SCALE_FACTOR);
    delete image;
    if (!resizedImage)
    {
        MessageBox(NULL, L"Failed to resize the image.", L"Error", MB_ICONERROR);
        GdiplusShutdown(g_GdiToken);
        return -1;
    }

    int width = resizedImage->GetWidth();
    int height = resizedImage->GetHeight();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = 0;
    int y = (screenHeight - height) + Y_OFFSET;

    const wchar_t CLASS_NAME[] = L"OverlayWindowClass";
    WNDCLASS wndClass = { 0 };
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = CLASS_NAME;
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClass(&wndClass))
    {
    }

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    HWND hWnd = CreateWindowEx(
        exStyle,
        CLASS_NAME,
        NULL,
        WS_POPUP,
        x, y, width, height,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hWnd)
    {
        MessageBox(NULL, L"Failed to create the window.", L"Error", MB_ICONERROR);
        delete resizedImage;
        GdiplusShutdown(g_GdiToken);
        return -1;
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    UpdateLayeredWindowBitmap(hWnd, resizedImage, x, y);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete resizedImage;
    GdiplusShutdown(g_GdiToken);
    return (int) msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

Bitmap* LoadAndProcessImage(const WCHAR* filename)
{
    Bitmap* bmp = new Bitmap(filename);
    if (bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        return nullptr;
    }

    if (bmp->GetPixelFormat() != PixelFormat32bppARGB)
    {
        Bitmap* converted = bmp->Clone(0, 0, bmp->GetWidth(), bmp->GetHeight(), PixelFormat32bppARGB);
        delete bmp;
        bmp = converted;
        if (!bmp)
            return nullptr;
    }

    Rect rect(0, 0, bmp->GetWidth(), bmp->GetHeight());
    BitmapData bmpData;
    if (bmp->LockBits(&rect, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &bmpData) != Ok)
    {
        delete bmp;
        return nullptr;
    }

    BYTE* pixels = static_cast<BYTE*>(bmpData.Scan0);
    int height = bmp->GetHeight();
    int width = bmp->GetWidth();
    for (int y = 0; y < height; y++)
    {
        BYTE* row = pixels + y * bmpData.Stride;
        for (int x = 0; x < width; x++)
        {
            BYTE* pixel = row + x * 4;
            BYTE blue = pixel[0];
            BYTE green = pixel[1];
            BYTE red = pixel[2];
            BYTE alpha = pixel[3];

            float alphaFactor = alpha / 255.0f;
            pixel[0] = static_cast<BYTE>(blue  * alphaFactor);
            pixel[1] = static_cast<BYTE>(green * alphaFactor);
            pixel[2] = static_cast<BYTE>(red   * alphaFactor);
        }
    }

    bmp->UnlockBits(&bmpData);
    return bmp;
}


Bitmap* ResizeImage(Bitmap* image, double scaleFactor)
{
    int originalW = image->GetWidth();
    int originalH = image->GetHeight();
    int newW = static_cast<int>(originalW * scaleFactor);
    int newH = static_cast<int>(originalH * scaleFactor);

    Bitmap* resized = new Bitmap(newW, newH, PixelFormat32bppARGB);
    if (!resized)
        return nullptr;

    Graphics graphics(resized);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.DrawImage(
        image,
        Rect(0, 0, newW, newH),
        0, 0, originalW, originalH,
        UnitPixel);

    return resized;
}

void UpdateLayeredWindowBitmap(HWND hWnd, Bitmap* image, int x, int y)
{
    int width = image->GetWidth();
    int height = image->GetHeight();

    Rect rect(0, 0, width, height);
    BitmapData bmpData;
    if (image->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok)
        return;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    if (hBitmap)
    {
        memcpy(pvBits, bmpData.Scan0, abs(bmpData.Stride) * height);

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        POINT ptSrc = { 0, 0 };
        SIZE sizeWnd = { width, height };
        POINT ptDst = { x, y };
        BLENDFUNCTION blend = { 0 };
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd,
                              hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmap);
    }

    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    image->UnlockBits(&bmpData);
}
