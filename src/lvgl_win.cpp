#include "lvgl_win.h"

#include <lvgl.h>

#include <objidl.h>
#include <gdiplus.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace Gdiplus;

static HWND g_hwnd = NULL;
static lv_display_t *g_disp = NULL;
static lv_indev_t *g_indev = NULL;
static void *g_buf = NULL;
static int g_bufW = 0;
static int g_bufH = 0;
static int g_mouseX = 0;
static int g_mouseY = 0;
static int g_mouseDown = 0;
static uint32_t g_lastTick = 0;
static int g_inited = 0;
static lv_font_t *g_font = NULL;
static lv_image_dsc_t g_bgDsc;
static uint8_t *g_bgPixels = NULL;
static UiConnectFn g_onConnect = NULL;
static UiQuitFn g_onQuit = NULL;

static lv_font_t *LoadUiFont(int pixelSize)
{
    char windir[MAX_PATH];
    char path[MAX_PATH * 2];
    FILE *f;
    long size;
    void *data;
    lv_font_t *font;

    if (GetWindowsDirectoryA(windir, sizeof(windir)) == 0) {
        return NULL;
    }
    _snprintf(path, sizeof(path), "%s\\Fonts\\segoeui.ttf", windir);
    path[sizeof(path) - 1] = '\0';
    f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)size);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    fread(data, 1, (size_t)size, f);
    fclose(f);
    font = lv_tiny_ttf_create_data(data, (size_t)size, pixelSize);
    if (font == NULL) {
        free(data);
    }
    return font;
}

static int LoadBackground(const char *gameDir)
{
    char path[MAX_PATH];
    wchar_t wpath[MAX_PATH];
    Bitmap *bmp;
    BitmapData data;
    Rect rect;
    int w;
    int h;
    int y;

    memset(&g_bgDsc, 0, sizeof(g_bgDsc));
    _snprintf(path, sizeof(path), "%s\\vellum\\resources\\images\\desktop.png", gameDir);
    path[sizeof(path) - 1] = '\0';
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    bmp = Bitmap::FromFile(wpath, FALSE);
    if (bmp == NULL || bmp->GetLastStatus() != Ok) {
        delete bmp;
        return 0;
    }
    w = (int)bmp->GetWidth();
    h = (int)bmp->GetHeight();
    rect = Rect(0, 0, w, h);
    memset(&data, 0, sizeof(data));
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
        delete bmp;
        return 0;
    }
    g_bgPixels = (uint8_t *)malloc((size_t)w * (size_t)h * 4u);
    if (g_bgPixels == NULL) {
        bmp->UnlockBits(&data);
        delete bmp;
        return 0;
    }
    for (y = 0; y < h; y++) {
        memcpy(g_bgPixels + (size_t)y * (size_t)w * 4u,
               (uint8_t *)data.Scan0 + (size_t)y * (size_t)data.Stride,
               (size_t)w * 4u);
    }
    bmp->UnlockBits(&data);
    delete bmp;

    g_bgDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    g_bgDsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    g_bgDsc.header.w = (uint32_t)w;
    g_bgDsc.header.h = (uint32_t)h;
    g_bgDsc.header.stride = (uint32_t)w * 4u;
    g_bgDsc.data_size = (uint32_t)w * (uint32_t)h * 4u;
    g_bgDsc.data = g_bgPixels;
    return 1;
}

static void FlushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

static void PointerReadCb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    data->point.x = (lv_coord_t)g_mouseX;
    data->point.y = (lv_coord_t)g_mouseY;
    data->state = g_mouseDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void OnConnectClicked(lv_event_t *e)
{
    (void)e;
    if (g_onConnect != NULL) {
        g_onConnect();
    }
}

static void OnQuitClicked(lv_event_t *e)
{
    (void)e;
    if (g_onQuit != NULL) {
        g_onQuit();
    }
}

static void BuildChrome(lv_obj_t *screen, int width, int height)
{
    lv_obj_t *img;
    lv_obj_t *panel;
    lv_obj_t *btn;
    lv_obj_t *label;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0c0c0c), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    if (g_bgPixels != NULL) {
        img = lv_image_create(screen);
        lv_image_set_src(img, &g_bgDsc);
        lv_obj_set_size(img, width, height);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }

    panel = lv_obj_create(screen);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(panel, 220, LV_SIZE_CONTENT);
    lv_obj_set_pos(panel, 24, 36);

    btn = lv_button_create(panel);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Главная");
    lv_obj_center(label);

    btn = lv_button_create(panel);
    lv_obj_set_width(btn, LV_PCT(100));
    label = lv_label_create(btn);
    lv_label_set_text(label, "Подключиться");
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, OnConnectClicked, LV_EVENT_CLICKED, NULL);

    btn = lv_button_create(panel);
    lv_obj_set_width(btn, LV_PCT(100));
    label = lv_label_create(btn);
    lv_label_set_text(label, "Выход");
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, OnQuitClicked, LV_EVENT_CLICKED, NULL);

    (void)height;
}

static void TeardownDisplay(void)
{
    if (g_indev != NULL) {
        lv_indev_delete(g_indev);
        g_indev = NULL;
    }
    if (g_disp != NULL) {
        lv_display_delete(g_disp);
        g_disp = NULL;
    }
    free(g_buf);
    g_buf = NULL;
    g_bufW = 0;
    g_bufH = 0;
}

static int CreateDisplay(int width, int height)
{
    size_t bufSize;
    lv_theme_t *theme;
    lv_obj_t *scr;

    if (width <= 0 || height <= 0) {
        return 0;
    }
    TeardownDisplay();

    bufSize = (size_t)width * (size_t)height * 4u;
    g_buf = malloc(bufSize);
    if (g_buf == NULL) {
        return 0;
    }
    g_bufW = width;
    g_bufH = height;

    g_disp = lv_display_create(width, height);
    lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(g_disp, g_buf, NULL, (uint32_t)bufSize, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(g_disp, FlushCb);

    theme = lv_theme_default_init(g_disp, lv_color_hex(0x3d8bfd), lv_color_hex(0x2a2a2a),
                                  true, g_font != NULL ? g_font : &lv_font_montserrat_14);
    lv_display_set_theme(g_disp, theme);

    g_indev = lv_indev_create();
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_indev, PointerReadCb);
    lv_indev_set_display(g_indev, g_disp);

    scr = lv_screen_active();
    BuildChrome(scr, width, height);
    return 1;
}

int Ui_Init(HWND hwnd, const char *gameDir)
{
    g_hwnd = hwnd;
    if (!g_inited) {
        lv_init();
        g_font = LoadUiFont(16);
        LoadBackground(gameDir);
        g_inited = 1;
        g_lastTick = GetTickCount();
    }
    return 1;
}

void Ui_SetHandlers(UiConnectFn onConnect, UiQuitFn onQuit)
{
    g_onConnect = onConnect;
    g_onQuit = onQuit;
}

void Ui_Resize(int width, int height)
{
    if (width == g_bufW && height == g_bufH && g_disp != NULL) {
        return;
    }
    CreateDisplay(width, height);
}

void Ui_Tick(void)
{
    uint32_t now = GetTickCount();
    uint32_t elapsed = now - g_lastTick;
    g_lastTick = now;
    if (g_disp == NULL) {
        return;
    }
    lv_tick_inc(elapsed);
    lv_timer_handler();
    if (g_hwnd != NULL) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

void Ui_Paint(HDC hdc, int width, int height)
{
    BITMAPINFO bmi;

    if (g_buf == NULL || g_bufW <= 0 || g_bufH <= 0) {
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = width;
        rc.bottom = height;
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return;
    }
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_bufW;
    bmi.bmiHeader.biHeight = -g_bufH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, 0, 0, width, height, 0, 0, g_bufW, g_bufH,
                  g_buf, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void Ui_OnMouse(int x, int y, int pressed)
{
    g_mouseX = x;
    g_mouseY = y;
    if (pressed >= 0) {
        g_mouseDown = pressed;
    }
}

void Ui_Shutdown(void)
{
    TeardownDisplay();
    free(g_bgPixels);
    g_bgPixels = NULL;
    g_onConnect = NULL;
    g_onQuit = NULL;
    g_hwnd = NULL;
}
