#include "Renderer.h"
#include "IconManager.h"
#include "core/PackageInfo.h"
#include <cassert>
#include <wrl/client.h>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Windowscodecs.lib")

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

uint64_t gradKey(uint32_t a, uint32_t b) {
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

D2D1_COLOR_F toColorF(uint32_t c) {
    D2D1_COLOR_F f;
    f.a = ((c >> 24) & 0xFF) / 255.0f;
    f.r = ((c >> 16) & 0xFF) / 255.0f;
    f.g = ((c >>  8) & 0xFF) / 255.0f;
    f.b = ( c        & 0xFF) / 255.0f;
    return f;
}

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

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wicFactory_.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    return createDeviceResources();
}

void Renderer::shutdown() {
    iconBitmaps_.clear();
    wicFactory_.Reset();
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

    renderTarget_ = hwndRT_;
    brushes_.clear();
    formats_.clear();
    linearBrushes_.clear();
    radialBrushes_.clear();
    return true;
}

void Renderer::discardDeviceResources() {
    renderTarget_.Reset();
    hwndRT_.Reset();
    brushes_.clear();
    formats_.clear();
    linearBrushes_.clear();
    radialBrushes_.clear();
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
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Scale(dpiScale_, dpiScale_));
    renderTarget_->BeginDraw();
}

void Renderer::endFrame() {
    if (!renderTarget_) return;
    HRESULT hr = renderTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discardDeviceResources();
        createDeviceResources();
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

ComPtr<ID2D1LinearGradientBrush> Renderer::getLinearBrush(uint32_t a, uint32_t b) {
    auto key = gradKey(a, b);
    auto it = linearBrushes_.find(key);
    if (it != linearBrushes_.end()) return it->second;
    if (!renderTarget_) return nullptr;
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, toColorF(a) },
        { 1.0f, toColorF(b) },
    };
    ComPtr<ID2D1GradientStopCollection> coll;
    HRESULT hr = renderTarget_->CreateGradientStopCollection(stops, 2, coll.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    ComPtr<ID2D1LinearGradientBrush> brush;
    hr = renderTarget_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(0, 0), D2D1::Point2F(0, 1)),
        coll.Get(), brush.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    linearBrushes_[key] = brush;
    return brush;
}

ComPtr<ID2D1RadialGradientBrush> Renderer::getRadialBrush(uint32_t a, uint32_t b) {
    auto key = gradKey(a, b);
    auto it = radialBrushes_.find(key);
    if (it != radialBrushes_.end()) return it->second;
    if (!renderTarget_) return nullptr;
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, toColorF(a) },
        { 1.0f, toColorF(b) },
    };
    ComPtr<ID2D1GradientStopCollection> coll;
    HRESULT hr = renderTarget_->CreateGradientStopCollection(stops, 2, coll.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    ComPtr<ID2D1RadialGradientBrush> brush;
    hr = renderTarget_->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(0, 0), D2D1::Point2F(0, 0), 1, 1),
        coll.Get(), brush.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    radialBrushes_[key] = brush;
    return brush;
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

float Renderer::measureTextWidth(const std::wstring& text, float fontSize, FontStyle style) {
    if (!dwriteFactory_ || text.empty()) return 0.0f;
    auto fmt = getFormat(fontSize, style);
    if (!fmt) return 0.0f;

    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = dwriteFactory_->CreateTextLayout(
        text.c_str(), static_cast<UINT32>(text.size()),
        fmt.Get(), 10000.0f, 1000.0f, layout.GetAddressOf());
    if (FAILED(hr)) return 0.0f;

    DWRITE_TEXT_METRICS metrics;
    layout->GetMetrics(&metrics);
    return metrics.width;
}

float Renderer::measureTextWidth(const std::string& text, float fontSize, FontStyle style) {
    return measureTextWidth(utf8ToWide(text), fontSize, style);
}

void Renderer::fillRectLinearV(const RectF& r, uint32_t colorTop, uint32_t colorBottom,
                               float radius) {
    if (!renderTarget_) return;
    auto brush = getLinearBrush(colorTop, colorBottom);
    if (!brush) return;
    brush->SetStartPoint(D2D1::Point2F(r.x, r.y));
    brush->SetEndPoint(D2D1::Point2F(r.x, r.y + r.h));
    if (radius > 0.0f) {
        renderTarget_->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h),
                              radius, radius),
            brush.Get());
    } else {
        renderTarget_->FillRectangle(
            D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get());
    }
}

void Renderer::fillRectLinearH(const RectF& r, uint32_t colorLeft, uint32_t colorRight,
                               float radius) {
    if (!renderTarget_) return;
    auto brush = getLinearBrush(colorLeft, colorRight);
    if (!brush) return;
    brush->SetStartPoint(D2D1::Point2F(r.x, r.y));
    brush->SetEndPoint(D2D1::Point2F(r.x + r.w, r.y));
    if (radius > 0.0f) {
        renderTarget_->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h),
                              radius, radius),
            brush.Get());
    } else {
        renderTarget_->FillRectangle(
            D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get());
    }
}

void Renderer::fillRectRadial(const RectF& r, float cx, float cy, float radiusFactor,
                              uint32_t colorCenter, uint32_t colorEdge) {
    if (!renderTarget_) return;
    auto brush = getRadialBrush(colorCenter, colorEdge);
    if (!brush) return;
    float rcx = r.x + r.w * cx;
    float rcy = r.y + r.h * cy;
    float rad = std::max(r.w, r.h) * radiusFactor;
    brush->SetCenter(D2D1::Point2F(rcx, rcy));
    brush->SetGradientOriginOffset(D2D1::Point2F(0, 0));
    brush->SetRadiusX(rad);
    brush->SetRadiusY(rad);
    renderTarget_->FillRectangle(
        D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h), brush.Get());
}

ComPtr<ID2D1Bitmap> Renderer::createBitmapFromHICON(HICON hIcon) {
    if (!wicFactory_ || !renderTarget_ || !hIcon) return nullptr;
    
    ComPtr<IWICBitmap> wicBitmap;
    HRESULT hr = wicFactory_->CreateBitmapFromHICON(hIcon, wicBitmap.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    hr = converter->Initialize(
        wicBitmap.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return nullptr;
    
    ComPtr<ID2D1Bitmap> d2dBitmap;
    hr = renderTarget_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, d2dBitmap.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    
    return d2dBitmap;
}

ComPtr<ID2D1Bitmap> Renderer::getIconBitmap(const std::string& packageId, const std::string& packageName, pm::PackageManager manager) {
    std::string cacheKey = packageId + "_" + std::to_string(static_cast<int>(manager));
    auto it = iconBitmaps_.find(cacheKey);
    if (it != iconBitmaps_.end()) return it->second;
    
    std::wstring path = IconManager::instance().findIconPath(packageId, packageName);
    if (!path.empty()) {
        HICON hIcon = IconManager::instance().extractIcon(path);
        if (hIcon) {
            ComPtr<ID2D1Bitmap> bmp = createBitmapFromHICON(hIcon);
            DestroyIcon(hIcon);
            if (bmp) {
                iconBitmaps_[cacheKey] = bmp;
                return bmp;
            }
        }
    }
    
    return nullptr;
}

void Renderer::drawIcon(const std::string& packageId, const std::string& packageName, pm::PackageManager manager, const RectF& rect) {
    ComPtr<ID2D1Bitmap> bmp = getIconBitmap(packageId, packageName, manager);
    if (bmp) {
        renderTarget_->DrawBitmap(
            bmp.Get(),
            D2D1::RectF(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h)
        );
    } else {
        // Draw Fluent vector fallback avatar
        uint32_t hash = 2166136261u;
        for (char c : packageId) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 16777619u;
        }
        
        static const uint32_t palette[] = {
            0xFF0078D4, 0xFF8764B8, 0xFF038387, 0xFF00B294,
            0xFFE81123, 0xFFCA5010, 0xFFB146C2, 0xFF1B85C5,
        };
        uint32_t base = (manager == pm::PackageManager::Winget)     ? 0xFF0078D4u
                      : (manager == pm::PackageManager::Scoop)      ? 0xFFBC5B00u
                      : (manager == pm::PackageManager::Chocolatey) ? 0xFF6B4423u
                      :                                           palette[hash % 8];
                      
        auto darkenColor = [](uint32_t c, float amt) -> uint32_t {
            uint8_t a = (c >> 24) & 0xFF;
            uint8_t r = (uint8_t)(((c >> 16) & 0xFF) * (1.0f - amt));
            uint8_t g = (uint8_t)(((c >>  8) & 0xFF) * (1.0f - amt));
            uint8_t b = (uint8_t)(( c        & 0xFF) * (1.0f - amt));
            return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | b;
        };
        
        // Base + brand top half
        fillRoundedRect(rect, darkenColor(base, 0.25f), 8.0f);
        fillRoundedRect({ rect.x, rect.y, rect.w, rect.h * 0.5f }, base, 8.0f);
        // Glossy highlight
        fillRectRadial(rect, 0.25f, 0.2f, 0.7f, 0x66FFFFFF, 0x00FFFFFF);
        // Outline
        strokeRect(rect, darkenColor(base, 0.45f), 1.0f, 8.0f);
        
        // Abbreviation
        std::string ab;
        if (!packageId.empty()) {
            auto upper = [](char c) { return (char)std::toupper((unsigned char)c); };
            auto dot = packageId.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < packageId.size()) {
                ab = std::string() + upper(packageId[0]) + upper(packageId[dot + 1]);
            } else if (packageId.size() >= 2) {
                ab = std::string() + upper(packageId[0]) + upper(packageId[1]);
            } else {
                ab = std::string(1, upper(packageId[0]));
            }
        } else {
            ab = "?";
        }
        
        drawText(ab, rect, 0xFFFFFFFF, 14.0f, Bold, true, true);
    }
}

} // namespace pm::gui::win32
