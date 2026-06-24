#include "Renderer.h"
#include <cassert>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace pm::gui::win32 {

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

uint64_t brushKey(uint32_t color) { return static_cast<uint64_t>(color); }

uint64_t formatKey(float size, Renderer::FontStyle s) {
    return (static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&size)) << 8) | static_cast<uint8_t>(s);
}

const wchar_t* familyName(Renderer::FontStyle s) {
    switch (s) {
        case Renderer::Bold:   return L"Segoe UI Semibold";
        case Renderer::Mono:   return L"Consolas";
        case Renderer::Icon:   return L"Segoe MDL2 Assets";
        case Renderer::Regular:
        default:               return L"Segoe UI";
    }
}

} // anonymous

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(HWND hwnd) {
    hwnd_ = hwnd;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                    d2dFactory_.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                              __uuidof(IDWriteFactory),
                              reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) return false;

    return createDeviceResources();
}

void Renderer::shutdown() {
    discardDeviceResources();
    dwriteFactory_.Reset();
    d2dFactory_.Reset();
    hwnd_ = nullptr;
}

bool Renderer::createDeviceResources() {
    if (!hwnd_ || !d2dFactory_) return false;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    width_  = static_cast<UINT>(rc.right - rc.left);
    height_ = static_cast<UINT>(rc.bottom - rc.top);
    if (width_ == 0 || height_ == 0) return false;

    dpiScale_ = 1.0f;
    if (auto dpi = GetDpiForWindow(hwnd_); dpi > 0) {
        dpiScale_ = dpi / 96.0f;
    }

    D2D1_SIZE_U pixelSize = D2D1::SizeU(width_, height_);
    HRESULT hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, pixelSize,
            D2D1_PRESENT_OPTIONS_IMMEDIATELY),
        hwndRT_.GetAddressOf());
    if (FAILED(hr)) return false;

    renderTarget_ = hwndRT_;  // implicit upcast to base class for drawing
    brushes_.clear();
    formats_.clear();
    return true;
}

void Renderer::discardDeviceResources() {
    renderTarget_.Reset();
    hwndRT_.Reset();
    brushes_.clear();
    formats_.clear();
}

bool Renderer::resize(UINT w, UINT h) {
    width_  = w;
    height_ = h;
    if (hwndRT_) {
        return SUCCEEDED(hwndRT_->Resize(D2D1::SizeU(w, h)));
    }
    return false;
}

void Renderer::beginFrame() {
    if (!renderTarget_) return;
    // Apply DPI scaling once per frame so all geometry passed to fill/draw/stroke
    // is in DIPs (logical pixels) instead of raw device pixels. Without this,
    // high-DPI displays (125%/150%/200%) render the entire UI too small.
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale_, dpiScale_));
    renderTarget_->BeginDraw();
}

void Renderer::endFrame() {
    if (!renderTarget_) return;
    HRESULT hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discardDeviceResources();
        createDeviceResources();
    } else {
        // Reset so a stale transform doesn't leak into the next frame if
        // beginFrame was skipped (e.g. first paint after device recreation).
        renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    }
}

void Renderer::clear(uint32_t color) {
    if (!renderTarget_) return;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color      ) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    renderTarget_->Clear(D2D1::ColorF(r, g, b, a));
}

ComPtr<ID2D1SolidColorBrush> Renderer::getBrush(uint32_t color) {
    auto key = brushKey(color);
    auto it = brushes_.find(key);
    if (it != brushes_.end()) return it->second;
    if (!renderTarget_) return nullptr;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color      ) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = renderTarget_->CreateSolidColorBrush(D2D1::ColorF(r, g, b, a), brush.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    brushes_[key] = brush;
    return brush;
}

ComPtr<IDWriteTextFormat> Renderer::getFormat(float size, FontStyle style) {
    auto key = formatKey(size, style);
    auto it = formats_.find(key);
    if (it != formats_.end()) return it->second;
    if (!dwriteFactory_) return nullptr;

    ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = dwriteFactory_->CreateTextFormat(
        familyName(style), nullptr,
        (style == Bold) ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        size, L"en-us", fmt.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    formats_[key] = fmt;
    return fmt;
}

void Renderer::fillRect(const RectF& r, uint32_t color, float radius) {
    if (!renderTarget_) return;
    auto brush = getBrush(color);
    if (!brush) return;
    if (radius > 0.0f) {
        renderTarget_->FillRoundedRectangle(
            D2D1::RoundedRect(toRect(r), radius, radius), brush.Get());
    } else {
        renderTarget_->FillRectangle(toRect(r), brush.Get());
    }
}

void Renderer::fillRoundedRect(const RectF& r, uint32_t color, float radius) {
    fillRect(r, color, radius);
}

void Renderer::strokeRect(const RectF& r, uint32_t color, float strokeWidth, float radius) {
    if (!renderTarget_) return;
    auto brush = getBrush(color);
    if (!brush) return;
    if (radius > 0.0f) {
        renderTarget_->DrawRoundedRectangle(
            D2D1::RoundedRect(toRect(r), radius, radius), brush.Get(),
            strokeWidth);
    } else {
        renderTarget_->DrawRectangle(toRect(r), brush.Get(), strokeWidth);
    }
}

void Renderer::drawLine(Vec2 from, Vec2 to, uint32_t color, float strokeWidth) {
    if (!renderTarget_) return;
    auto brush = getBrush(color);
    if (!brush) return;
    renderTarget_->DrawLine(toPoint(from), toPoint(to), brush.Get(), strokeWidth);
}

void Renderer::drawText(const std::wstring& text, const RectF& rect, uint32_t color,
                        float fontSize, FontStyle style, bool centerX, bool centerY) {
    if (!renderTarget_ || text.empty()) return;
    auto brush = getBrush(color);
    if (!brush) return;
    auto fmt = getFormat(fontSize, style);
    if (!fmt) return;
    D2D1_RECT_F layout = toRect(rect);
    if (centerX) fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    else         fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (centerY) fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    else         fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    renderTarget_->DrawText(text.c_str(), static_cast<UINT32>(text.size()),
                            fmt.Get(), &layout, brush.Get(),
                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Renderer::drawText(const std::string& text, const RectF& rect, uint32_t color,
                        float fontSize, FontStyle style, bool centerX, bool centerY) {
    drawText(utf8ToWide(text), rect, color, fontSize, style, centerX, centerY);
}

} // namespace pm::gui::win32
