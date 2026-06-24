#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <functional>

namespace pm::gui::win32 {

using Microsoft::WRL::ComPtr;

struct RectF { float x, y, w, h; };
struct Vec2  { float x, y; };

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(HWND hwnd);
    void shutdown();

    void beginFrame();
    void endFrame();

    bool resize(UINT w, UINT h);

    float dpiScale() const { return dpiScale_; }

    // ---- Primitives ----
    void clear(uint32_t color);
    void fillRect(const RectF& r, uint32_t color, float radius = 0.0f);
    void strokeRect(const RectF& r, uint32_t color, float strokeWidth, float radius = 0.0f);
    void fillRoundedRect(const RectF& r, uint32_t color, float radius);
    void drawLine(Vec2 from, Vec2 to, uint32_t color, float strokeWidth = 1.0f);

    // ---- Text ----
    enum FontStyle { Regular = 0, Bold = 1, Mono = 2, Icon = 3 };
    void drawText(const std::wstring& text, const RectF& rect, uint32_t color,
                  float fontSize, FontStyle style = Regular,
                  bool centerX = false, bool centerY = false);
    void drawText(const std::string& text, const RectF& rect, uint32_t color,
                  float fontSize, FontStyle style = Regular,
                  bool centerX = false, bool centerY = false);

    HWND hwnd() const { return hwnd_; }

    // Direct D2D conversion helpers (inline for cheap use).
    static D2D1_POINT_2F toPoint(Vec2 v) { return D2D1::Point2F(v.x, v.y); }
    static D2D1_RECT_F   toRect(RectF r) { return D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h); }

private:
    bool createDeviceResources();
    void discardDeviceResources();

    ComPtr<ID2D1Factory>      d2dFactory_;
    // Base class for drawing methods; underlying is ID2D1HwndRenderTarget.
    ComPtr<ID2D1RenderTarget> renderTarget_;
    // Hwnd-specific pointer for Resize (which only exists on HwndRenderTarget).
    ComPtr<ID2D1HwndRenderTarget> hwndRT_;
    ComPtr<IDWriteFactory>    dwriteFactory_;

    ComPtr<ID2D1SolidColorBrush> getBrush(uint32_t color);
    ComPtr<IDWriteTextFormat>   getFormat(float size, FontStyle style);

    std::unordered_map<uint64_t, ComPtr<ID2D1SolidColorBrush>> brushes_;
    std::unordered_map<uint64_t, ComPtr<IDWriteTextFormat>>   formats_;

    HWND    hwnd_     = nullptr;
    float   dpiScale_ = 1.0f;
    UINT    width_    = 0;
    UINT    height_   = 0;
};

} // namespace pm::gui::win32
