#include "UsernamePrompt.hpp"

#include <cctype>

#include <windows.h>

namespace kungfu {

namespace {

constexpr int kEditId = 101;
constexpr int kPlayButtonId = 102;
constexpr int kWindowWidth = 320;
constexpr int kWindowHeight = 160;
constexpr size_t kMaxUsernameLength = 20;

// Owns the one Edit control's handle and the outcome of the prompt - set from WndProc,
// read back by show() once the message loop exits. One of these per show() call; never
// shared or reused, so there's no lifetime concern beyond the single blocking call.
struct PromptState {
    HWND edit = nullptr;
    std::optional<std::string> result;  // set on Play; stays nullopt if the window is closed instead
    bool done = false;
};

PromptState* stateFor(HWND hwnd) {
    return reinterpret_cast<PromptState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

// The wire protocol is space-delimited text, so a username can't contain whitespace -
// strip it rather than reject it, so a space-happy typist still gets *something* sent.
std::string sanitize(const std::string& raw) {
    std::string cleaned;
    cleaned.reserve(raw.size());
    for (char c : raw) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleaned += c;
        }
    }
    if (cleaned.size() > kMaxUsernameLength) {
        cleaned.resize(kMaxUsernameLength);
    }
    return cleaned;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<PromptState*>(cs->lpCreateParams);

            CreateWindowExA(0, "STATIC", "Enter a username:", WS_CHILD | WS_VISIBLE, 20, 20, 260, 20, hwnd,
                             nullptr, cs->hInstance, nullptr);
            state->edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           20, 45, 260, 24, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)), cs->hInstance,
                                           nullptr);
            CreateWindowExA(0, "BUTTON", "Play", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 100, 90, 100, 30, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayButtonId)), cs->hInstance, nullptr);
            SetFocus(state->edit);
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) != kPlayButtonId) {
                break;
            }
            auto* state = stateFor(hwnd);
            if (!state) {
                break;
            }
            char buffer[256] = {};
            GetWindowTextA(state->edit, buffer, sizeof(buffer));
            state->result = sanitize(buffer);
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            auto* state = stateFor(hwnd);
            if (state) {
                state->done = true;
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

}  // namespace

std::optional<std::string> UsernamePrompt::show() {
    PromptState state;

    const char* className = "KungFuChessUsernamePrompt";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Join", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                 CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight, nullptr, nullptr,
                                 wc.hInstance, &state);
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA(className, wc.hInstance);
    return state.result;
}

}  // namespace kungfu
