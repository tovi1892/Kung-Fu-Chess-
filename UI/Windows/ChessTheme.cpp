#include "ChessTheme.hpp"

namespace kungfu {

namespace {
constexpr int kFontHeight = -16;      // negative: character height in pixels, not cell height
constexpr int kTitleFontHeight = -22;
const char* const kFontFace = "Georgia";
}  // namespace

ChessTheme::ChessTheme() {
    font_ = CreateFontA(kFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_ROMAN, kFontFace);
    titleFont_ = CreateFontA(kTitleFontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_ROMAN, kFontFace);
    backgroundBrush_ = CreateSolidBrush(toColorRef(kBackground));
    fieldBrush_ = CreateSolidBrush(toColorRef(kFieldBackground));
    buttonBrush_ = CreateSolidBrush(toColorRef(kButton));
    buttonDisabledBrush_ = CreateSolidBrush(toColorRef(kButtonDisabled));
}

ChessTheme::~ChessTheme() {
    if (font_) DeleteObject(font_);
    if (titleFont_) DeleteObject(titleFont_);
    if (backgroundBrush_) DeleteObject(backgroundBrush_);
    if (fieldBrush_) DeleteObject(fieldBrush_);
    if (buttonBrush_) DeleteObject(buttonBrush_);
    if (buttonDisabledBrush_) DeleteObject(buttonDisabledBrush_);
}

HBRUSH ChessTheme::paintStatic(HDC hdc, bool isError) const {
    SetTextColor(hdc, toColorRef(isError ? kError : kText));
    SetBkMode(hdc, TRANSPARENT);
    return backgroundBrush_;
}

HBRUSH ChessTheme::paintEdit(HDC hdc) const {
    SetTextColor(hdc, toColorRef(kFieldText));
    SetBkColor(hdc, toColorRef(kFieldBackground));
    return fieldBrush_;
}

void ChessTheme::drawButton(HDC hdc, const RECT& rect, const std::string& text, bool enabled) const {
    FillRect(hdc, &rect, enabled ? buttonBrush_ : buttonDisabledBrush_);
    SetTextColor(hdc, toColorRef(kButtonText));
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, font_);
    RECT textRect = rect;
    DrawTextA(hdc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

}  // namespace kungfu
