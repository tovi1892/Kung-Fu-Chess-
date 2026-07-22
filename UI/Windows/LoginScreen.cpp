#include "LoginScreen.hpp"

#include <cctype>
#include <iostream>

#include <windows.h>

#include "ChessTheme.hpp"
#include "OpenCV/RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kUsernameEditId = 101;
constexpr int kPasswordEditId = 102;
constexpr int kSignInButtonId = 103;
constexpr size_t kMaxUsernameLength = 20;
constexpr size_t kMaxPasswordLength = 64;

// The wire protocol is space-delimited text, so a username/password can't contain
// whitespace - strip it rather than reject it, so a space-happy typist still gets
// *something* sent. Same rule UsernamePrompt/RoomChoiceScreen/PlayConfirmScreen all apply
// to their own text fields.
std::string sanitize(const std::string& raw, size_t maxLength) {
    std::string cleaned;
    cleaned.reserve(raw.size());
    for (char c : raw) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleaned += c;
        }
    }
    if (cleaned.size() > maxLength) {
        cleaned.resize(maxLength);
    }
    return cleaned;
}

// --- Main login window --------------------------------------------------------------

// Owns the window's control handles, the theme it paints with, and the outcome of the
// prompt - set from WndProc, read back by show() once the message loop exits. One of these
// per show() call; never shared or reused, so there's no lifetime concern beyond the single
// blocking call.
struct LoginState {
    const ChessTheme* theme = nullptr;
    HWND usernameEdit = nullptr;
    HWND passwordEdit = nullptr;
    HWND errorLabel = nullptr;
    HWND signInButton = nullptr;
    std::string errorMessage;     // shown once, at WM_CREATE - not updated live
    std::string prefillUsername;  // shown once, at WM_CREATE - not updated live
    std::optional<std::pair<std::string, std::string>> result;
    bool done = false;
};

LoginState* stateFor(HWND hwnd) {
    return reinterpret_cast<LoginState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<LoginState*>(cs->lpCreateParams);
            const ChessTheme& theme = *state->theme;

            HWND title = CreateWindowExA(0, "STATIC", "Kung Fu Chess", WS_CHILD | WS_VISIBLE,
                                          RenderConfig::kLoginScreenTitleX, RenderConfig::kLoginScreenTitleY,
                                          RenderConfig::kLoginScreenTitleWidth, RenderConfig::kLoginScreenTitleHeight,
                                          hwnd, nullptr, cs->hInstance, nullptr);
            SendMessage(title, WM_SETFONT, reinterpret_cast<WPARAM>(theme.titleFont()), TRUE);

            HWND usernameLabel = CreateWindowExA(0, "STATIC", "Username:", WS_CHILD | WS_VISIBLE,
                                                  RenderConfig::kLoginScreenLabelX, RenderConfig::kLoginScreenUsernameLabelY,
                                                  RenderConfig::kLoginScreenLabelWidth, RenderConfig::kLoginScreenLabelHeight,
                                                  hwnd, nullptr, cs->hInstance, nullptr);
            SendMessage(usernameLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->usernameEdit =
                CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", state->prefillUsername.c_str(),
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, RenderConfig::kLoginScreenEditX,
                                 RenderConfig::kLoginScreenUsernameEditY, RenderConfig::kLoginScreenEditWidth,
                                 RenderConfig::kLoginScreenEditHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kUsernameEditId)), cs->hInstance, nullptr);
            SendMessage(state->usernameEdit, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            HWND passwordLabel = CreateWindowExA(0, "STATIC", "Password:", WS_CHILD | WS_VISIBLE,
                                                  RenderConfig::kLoginScreenLabelX, RenderConfig::kLoginScreenPasswordLabelY,
                                                  RenderConfig::kLoginScreenLabelWidth, RenderConfig::kLoginScreenLabelHeight,
                                                  hwnd, nullptr, cs->hInstance, nullptr);
            SendMessage(passwordLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            // ES_PASSWORD masks every typed character as '*' - the one thing that needs to
            // be explicit here, everything else is identical to the username EDIT above.
            state->passwordEdit =
                CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                 RenderConfig::kLoginScreenEditX, RenderConfig::kLoginScreenPasswordEditY,
                                 RenderConfig::kLoginScreenEditWidth, RenderConfig::kLoginScreenEditHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPasswordEditId)), cs->hInstance, nullptr);
            SendMessage(state->passwordEdit, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->errorLabel = CreateWindowExA(0, "STATIC", state->errorMessage.c_str(), WS_CHILD | WS_VISIBLE,
                                                 RenderConfig::kLoginScreenLabelX, RenderConfig::kLoginScreenErrorLabelY,
                                                 RenderConfig::kLoginScreenLabelWidth,
                                                 RenderConfig::kLoginScreenErrorLabelHeight, hwnd, nullptr, cs->hInstance,
                                                 nullptr);
            SendMessage(state->errorLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->signInButton =
                CreateWindowExA(0, "BUTTON", "Sign In", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 RenderConfig::kLoginScreenButtonX, RenderConfig::kLoginScreenButtonY,
                                 RenderConfig::kLoginScreenButtonWidth, RenderConfig::kLoginScreenButtonHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSignInButtonId)), cs->hInstance, nullptr);

            // If the username is already prefilled (a retry after a wrong password), the
            // password is almost certainly the field that actually needs retyping - so
            // focus there instead of making the user re-click past the username they
            // already got right.
            SetFocus(state->prefillUsername.empty() ? state->usernameEdit : state->passwordEdit);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            return reinterpret_cast<INT_PTR>(state->theme->paintStatic(hdc, control == state->errorLabel));
        }
        case WM_CTLCOLOREDIT: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            HDC hdc = reinterpret_cast<HDC>(wParam);
            return reinterpret_cast<INT_PTR>(state->theme->paintEdit(hdc));
        }
        case WM_DRAWITEM: {
            auto* state = stateFor(hwnd);
            if (!state || wParam != static_cast<WPARAM>(kSignInButtonId)) break;
            auto* dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            state->theme->drawButton(dis->hDC, dis->rcItem, "Sign In", true);
            return TRUE;
        }
        case WM_COMMAND: {
            auto* state = stateFor(hwnd);
            if (!state) break;

            if (LOWORD(wParam) == kSignInButtonId) {
                char userBuffer[256] = {};
                GetWindowTextA(state->usernameEdit, userBuffer, sizeof(userBuffer));
                const std::string username = sanitize(userBuffer, kMaxUsernameLength);

                char passBuffer[256] = {};
                GetWindowTextA(state->passwordEdit, passBuffer, sizeof(passBuffer));
                const std::string password = sanitize(passBuffer, kMaxPasswordLength);

                if (username.empty()) {
                    SetFocus(state->usernameEdit);
                    return 0;  // an empty username is a no-op - stay on this window
                }

                state->result = std::make_pair(username, password);
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_DESTROY: {
            // No PostQuitMessage here: this loop's exit condition is state->done, not
            // WM_QUIT - LoginScreen creates further windows (showResult(), and later
            // RoomChoiceScreen/PlayConfirmScreen) on this same thread, and a WM_QUIT posted
            // here would sit unconsumed (this loop already stops via state->done before
            // calling GetMessage again) and poison the *next* window's first GetMessage
            // call instead, making it return immediately without ever pumping that new
            // window's messages.
            auto* state = stateFor(hwnd);
            if (state) {
                state->done = true;
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Used only if the native window can't be created at all (see show()) - so the app always
// has *some* way to get a username instead of silently hanging forever with no visible
// window and no explanation.
std::optional<std::pair<std::string, std::string>> promptOnConsole() {
    std::cout << "(Couldn't open a native window here - type your credentials instead.)\n"
                 "Username (blank to cancel): "
              << std::flush;
    std::string usernameLine;
    if (!std::getline(std::cin, usernameLine)) {
        return std::nullopt;
    }
    const std::string username = sanitize(usernameLine, kMaxUsernameLength);
    if (username.empty()) {
        return std::nullopt;
    }

    // No portable console-masking without extra platform code - accepted as a minor gap in
    // an already-degraded last-resort path (the native window is always tried first).
    std::cout << "Password (visible - this fallback has no input masking): " << std::flush;
    std::string passwordLine;
    if (!std::getline(std::cin, passwordLine)) {
        return std::nullopt;
    }
    return std::make_pair(username, sanitize(passwordLine, kMaxPasswordLength));
}

// --- Result window: shown once LOGIN_OK is known --------------------------------------

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
            // See LoginWndProc's WM_DESTROY comment: no PostQuitMessage here either, same
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

std::optional<std::pair<std::string, std::string>> LoginScreen::show(const std::string& errorMessage,
                                                                       const std::string& prefillUsername) {
    ChessTheme theme;
    LoginState state;
    state.theme = &theme;
    state.errorMessage = errorMessage;
    state.prefillUsername = prefillUsername;

    const char* className = "KungFuChessLoginScreen";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = LoginWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme.backgroundBrush();

    if (!RegisterClassA(&wc)) {
        std::cerr << "[LoginScreen] RegisterClassA failed, error " << GetLastError() << std::endl;
        return promptOnConsole();
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Sign In",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                 RenderConfig::kLoginScreenWindowWidth, RenderConfig::kLoginScreenWindowHeight, nullptr,
                                 nullptr, wc.hInstance, &state);
    if (!hwnd) {
        std::cerr << "[LoginScreen] CreateWindowExA failed, error " << GetLastError() << std::endl;
        UnregisterClassA(className, wc.hInstance);
        return promptOnConsole();
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA(className, wc.hInstance);
    return state.result;
}

void LoginScreen::showResult(const std::string& username, int rating, bool accountCreated) {
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
        std::cerr << "[LoginScreen] Result RegisterClassA failed, error " << GetLastError() << std::endl;
        std::cout << state.message << std::endl;
        return;
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT, RenderConfig::kLoginResultWindowWidth,
                                 RenderConfig::kLoginResultWindowHeight, nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) {
        std::cerr << "[LoginScreen] Result CreateWindowExA failed, error " << GetLastError() << std::endl;
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
