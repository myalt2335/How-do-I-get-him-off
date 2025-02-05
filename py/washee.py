import win32api
import win32con
import win32gui
import ctypes
from PIL import Image
import sys
import os
import numpy as np

SCALE_FACTOR = 0.65
Y_OFFSET = -38

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32),
        ("biWidth", ctypes.c_long),
        ("biHeight", ctypes.c_long),
        ("biPlanes", ctypes.c_ushort),
        ("biBitCount", ctypes.c_ushort),
        ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32),
        ("biXPelsPerMeter", ctypes.c_long),
        ("biYPelsPerMeter", ctypes.c_long),
        ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]

def premultiply_alpha(image):
    img_array = np.array(image, dtype=np.float32)

    r, g, b, a = img_array[:, :, 0], img_array[:, :, 1], img_array[:, :, 2], img_array[:, :, 3]
    alpha_factor = a / 255.0

    r = (r * alpha_factor).astype(np.uint8)
    g = (g * alpha_factor).astype(np.uint8)
    b = (b * alpha_factor).astype(np.uint8)

    premultiplied = np.dstack((r, g, b, a)).astype(np.uint8)
    return Image.fromarray(premultiplied, "RGBA")

def resize_image(image, scale_factor):
    w, h = image.size
    new_size = (int(w * scale_factor), int(h * scale_factor))

    rgb = image.convert("RGB").resize(new_size, Image.LANCZOS)
    alpha = image.getchannel("A").resize(new_size, Image.LANCZOS)

    resized_image = Image.merge("RGBA", (*rgb.split(), alpha))
    return resized_image

def update_layered_window(hWnd, image, x, y):
    width, height = image.size
    image_data = image.tobytes("raw", "BGRA")

    hdcScreen = win32gui.GetDC(0)
    hdcMem = win32gui.CreateCompatibleDC(hdcScreen)

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = width
    bmi.biHeight = -height
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = win32con.BI_RGB

    ppvBits = ctypes.c_void_p()
    hBitmap = ctypes.windll.gdi32.CreateDIBSection(hdcScreen, ctypes.byref(bmi), win32con.DIB_RGB_COLORS, ctypes.byref(ppvBits), None, 0)
    if not hBitmap:
        win32gui.ReleaseDC(hWnd, hdcScreen)
        return

    ctypes.memmove(ppvBits, image_data, len(image_data))
    win32gui.SelectObject(hdcMem, hBitmap)

    blend = (win32con.AC_SRC_OVER, 0, 255, win32con.AC_SRC_ALPHA)

    win32gui.UpdateLayeredWindow(hWnd, hdcScreen, (x, y), (width, height), hdcMem, (0, 0), 0, blend, win32con.ULW_ALPHA)

    win32gui.DeleteObject(hBitmap)
    win32gui.DeleteDC(hdcMem)
    win32gui.ReleaseDC(hWnd, hdcScreen)

def wnd_proc(hWnd, msg, wParam, lParam):
    if msg == win32con.WM_DESTROY:
        win32gui.PostQuitMessage(0)
        return 0
    return win32gui.DefWindowProc(hWnd, msg, wParam, lParam)

def main():
    image_path = "mrwashee.png"
    if not os.path.exists(image_path):
        sys.exit(1)

    try:
        image = Image.open(image_path).convert("RGBA")
    except Exception:
        sys.exit(1)

    image = premultiply_alpha(image)
    resized_image = resize_image(image, SCALE_FACTOR)

    width, height = resized_image.size
    screen_width = win32api.GetSystemMetrics(win32con.SM_CXSCREEN)
    screen_height = win32api.GetSystemMetrics(win32con.SM_CYSCREEN)
    x = 0
    y = (screen_height - height) + Y_OFFSET

    className = "OverlayWindow"
    wndClass = win32gui.WNDCLASS()
    hInstance = win32api.GetModuleHandle(None)
    wndClass.hInstance = hInstance
    wndClass.lpszClassName = className
    wndClass.style = win32con.CS_HREDRAW | win32con.CS_VREDRAW
    wndClass.hCursor = win32gui.LoadCursor(0, win32con.IDC_ARROW)
    wndClass.lpfnWndProc = wnd_proc

    try:
        win32gui.RegisterClass(wndClass)
    except Exception:
        pass

    exStyle = win32con.WS_EX_LAYERED | win32con.WS_EX_TRANSPARENT | win32con.WS_EX_TOPMOST | win32con.WS_EX_NOACTIVATE
    hWnd = win32gui.CreateWindowEx(exStyle, className, None, win32con.WS_POPUP, x, y, width, height, 0, 0, hInstance, None)

    win32gui.ShowWindow(hWnd, win32con.SW_SHOW)
    win32gui.UpdateWindow(hWnd)
    update_layered_window(hWnd, resized_image, x, y)

    win32gui.PumpMessages()

if __name__ == "__main__":
    main()
