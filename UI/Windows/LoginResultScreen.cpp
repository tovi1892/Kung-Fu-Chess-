#include "LoginResultScreen.hpp"

#include <iostream>

#include <windows.h>

#include "ChessTheme.hpp"
#include "OpenCV/RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kContinueButtonId = 201;

struct ResultState {
    const ChessTheme* theme = nullptr;
    std::string message;
    HWND continueButton = nullptr;
    bool done = false;
};

ResultState* resultStateFor(HWND hwnd) {
    return reinterpret_cast<ResultState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK ResultWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<ResultState*>(cs->lpCreateParams);
            const ChessTheme& theme = *state->theme;

            HWND message = CreateWindowExA(0, "STATIC", state->message.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
                                            RenderConfig::kLoginResultMessageX, RenderConfig::kLoginResultMessageY,
                                            RenderConfig::kLoginResultMessageWidth,
                                            RenderConfig::kLoginResultMessageHeight, hwnd, nullptr, cs->hInstance,
                                            nullptr);
            SendMessage(message, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->continueButton =
                CreateWindowExA(0, "BUTTON", "Continue", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 RenderConfig::kLoginResultButtonX, RenderConfig::kLoginResultButtonY,
                                 RenderConfig::kLoginResultButtonWidth, RenderConfig::kLoginResultButtonHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kContinueButtonId)), cs->hInstance,
                                 nullptr);
            SetFocus(state->continueButton);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            auto* state = resultStateFor(hwnd);
            if (!state) break;
            return reinterpret_cast<INT_PTR>(state->theme->paintStatic(reinterpret_cast<HDC>(wParam)));
        }
        case WM_DRAWITEM: {
            auto* state = resultStateFor(hwnd);
            if (!state || wParam != static_cast<WPARAM>(kContinueButtonId)) break;
            auto* dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            state->theme->drawButton(dis->hDC, dis->rcItem, "Continue", true);
            return TRUE;
        }
        case WM_COMMAND: {
            auto* state = resultStateFor(hwnd);
            if (state && LOWORD(wParam) == kContinueButtonId) {
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_DESTROY: {
            // See LoginScreen.cpp's WM_DESTROY comment: no PostQuitMessage here either, same
            // reasoning - this loop already exits via state->done.
            auto* state = resultStateFor(hwnd);
            if (state) {
                state->done = true;
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

}  // namespace

void LoginResultScreen::show(const std::string& username, int rating, bool accountCreated) {
    ChessTheme theme;
    ResultState state;
    state.theme = &theme;
    state.message = (accountCreated ? "Account created for " : "Welcome back, ") + username + " (rating " +
                     std::to_string(rating) + ")";

    const char* className = "KungFuChessLoginResult";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = ResultWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme.backgroundBrush();

    if (!RegisterClassA(&wc)) {
        std::cerr << "[LoginResultScreen] RegisterClassA failed, error " << GetLastError() << std::endl;
        std::cout << state.message << std::endl;
        return;
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, RenderConfig::kLoginResultWindowWidth,
                                 RenderConfig::kLoginResultWindowHeight, nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) {
        std::cerr << "[LoginResultScreen] CreateWindowExA failed, error " << GetLastError() << std::endl;
        UnregisterClassA(className, wc.hInstance);
        std::cout << state.message << std::endl;
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA(className, wc.hInstance);
}

}  // namespace kungfu
