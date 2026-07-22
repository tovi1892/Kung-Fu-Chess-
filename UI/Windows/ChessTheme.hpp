#pragma once

#include <string>

#include <windows.h>

#include "Img/Color.hpp"

namespace kungfu {

// Chess-appropriate look for the Win32 login/room/play-confirm screens (LoginScreen,
// RoomChoiceScreen, PlayConfirmScreen): a shared color palette (as Color - already
// OpenCV-free, so the same values could in principle be reused by the OpenCV-rendered
// board too, not two unrelated palettes), cached fonts, and the brushes each screen's own
// WM_CTLCOLOR*/WM_DRAWITEM handlers paint controls with. One responsibility - how this
// flow looks, not how it behaves - screens still own their own window procs and message
// loops, they just call into this instead of each defining their own GDI objects.
class ChessTheme {
public:
    ChessTheme();
    ~ChessTheme();

    ChessTheme(const ChessTheme&) = delete;
    ChessTheme& operator=(const ChessTheme&) = delete;

    // Dark walnut-board background, light tan (light-square-like) input fields, a rust
    // accent for primary buttons - the same rust family BoardRenderer's rest-ring already
    // uses (kStartColor), so the dialogs and the board don't read as two unrelated apps.
    static constexpr Color kBackground{34, 28, 24};
    static constexpr Color kFieldBackground{238, 214, 175};
    static constexpr Color kFieldText{40, 30, 20};
    static constexpr Color kText{230, 222, 210};
    static constexpr Color kButton{150, 80, 30};
    static constexpr Color kButtonDisabled{90, 85, 78};
    static constexpr Color kButtonText{245, 240, 232};
    static constexpr Color kError{224, 110, 96};

    HFONT font() const { return font_; }
    HFONT titleFont() const { return titleFont_; }

    // The window/dialog's own background brush - for WM_ERASEBKGND or as the window
    // class's hbrBackground.
    HBRUSH backgroundBrush() const { return backgroundBrush_; }

    // Call from a window's WM_CTLCOLORSTATIC handler: configures hdc's text color/mode for
    // this theme (isError picks the error-red text color, otherwise the normal light text
    // color) and returns the brush to paint that control's background with.
    HBRUSH paintStatic(HDC hdc, bool isError = false) const;

    // Call from a window's WM_CTLCOLOREDIT handler.
    HBRUSH paintEdit(HDC hdc) const;

    // Call from a window's WM_DRAWITEM handler for an owner-drawn (BS_OWNERDRAW) button -
    // fills it with the accent (or disabled) color and draws centered text.
    void drawButton(HDC hdc, const RECT& rect, const std::string& text, bool enabled) const;

    static COLORREF toColorRef(const Color& c) { return RGB(c.r, c.g, c.b); }

private:
    HFONT font_ = nullptr;
    HFONT titleFont_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH fieldBrush_ = nullptr;
    HBRUSH buttonBrush_ = nullptr;
    HBRUSH buttonDisabledBrush_ = nullptr;
};

}  // namespace kungfu
