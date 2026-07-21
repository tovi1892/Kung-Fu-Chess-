#include "UsernamePrompt.hpp"

#include <cctype>
#include <iostream>

#include <windows.h>

#include "OpenCV/RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kEditId = 101;
constexpr int kPlayButtonId = 102;
constexpr int kRoomButtonId = 103;
constexpr int kPasswordEditId = 104;
constexpr size_t kMaxUsernameLength = 20;
constexpr size_t kMaxRoomLength = 20;
constexpr size_t kMaxPasswordLength = 64;

// Owns the main window's two Edit controls' handles, the outcome of the prompt, and the
// error message (if any) to display - set from WndProc, read back by show() once the
// message loop exits. One of these per show() call; never shared or reused, so there's no
// lifetime concern beyond the single blocking call.
struct PromptState {
    HWND edit = nullptr;
    HWND passwordEdit = nullptr;
    std::string errorMessage;                // shown once, at WM_CREATE - not updated live
    std::optional<JoinInfo> result;  // set on Play/Room; stays nullopt if the window is closed instead
    bool done = false;
};

PromptState* stateFor(HWND hwnd) {
    return reinterpret_cast<PromptState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

// The wire protocol is space-delimited text, so a username/password/room id can't contain
// whitespace - strip it rather than reject it, so a space-happy typist still gets
// *something* sent. Shared by the username field, the password field, and the room
// popup's edit box.
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

// --- Room popup: opened by the main window's Room button ------------------------------

struct RoomPopupResult {
    bool create = false;
    std::string typedRoom;  // meaningful only when !create
};

struct RoomPromptState {
    HWND edit = nullptr;
    enum class Outcome { Cancelled, Create, Join } outcome = Outcome::Cancelled;
    std::string roomText;
    bool done = false;
};

RoomPromptState* roomStateFor(HWND hwnd) {
    return reinterpret_cast<RoomPromptState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

constexpr int kRoomEditId = 201;
constexpr int kCreateButtonId = 202;
constexpr int kJoinButtonId = 203;
constexpr int kCancelButtonId = 204;

LRESULT CALLBACK RoomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<RoomPromptState*>(cs->lpCreateParams);

            CreateWindowExA(0, "STATIC", "Room id (blank for Create):", WS_CHILD | WS_VISIBLE,
                             RenderConfig::kRoomPromptLabelX, RenderConfig::kRoomPromptLabelY,
                             RenderConfig::kRoomPromptLabelWidth, RenderConfig::kRoomPromptLabelHeight, hwnd,
                             nullptr, cs->hInstance, nullptr);
            state->edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           RenderConfig::kRoomPromptEditX, RenderConfig::kRoomPromptEditY,
                                           RenderConfig::kRoomPromptEditWidth, RenderConfig::kRoomPromptEditHeight,
                                           hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRoomEditId)),
                                           cs->hInstance, nullptr);
            CreateWindowExA(0, "BUTTON", "Create", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                             RenderConfig::kRoomPromptCreateButtonX, RenderConfig::kRoomPromptButtonY,
                             RenderConfig::kRoomPromptButtonWidth, RenderConfig::kRoomPromptButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCreateButtonId)), cs->hInstance, nullptr);
            CreateWindowExA(0, "BUTTON", "Join", WS_CHILD | WS_VISIBLE,
                             RenderConfig::kRoomPromptJoinButtonX, RenderConfig::kRoomPromptButtonY,
                             RenderConfig::kRoomPromptButtonWidth, RenderConfig::kRoomPromptButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kJoinButtonId)), cs->hInstance, nullptr);
            CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
                             RenderConfig::kRoomPromptCancelButtonX, RenderConfig::kRoomPromptButtonY,
                             RenderConfig::kRoomPromptButtonWidth, RenderConfig::kRoomPromptButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelButtonId)), cs->hInstance, nullptr);
            SetFocus(state->edit);
            return 0;
        }
        case WM_COMMAND: {
            auto* state = roomStateFor(hwnd);
            if (!state) {
                break;
            }
            switch (LOWORD(wParam)) {
                case kCreateButtonId:
                    state->outcome = RoomPromptState::Outcome::Create;
                    state->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                case kJoinButtonId: {
                    char buffer[256] = {};
                    GetWindowTextA(state->edit, buffer, sizeof(buffer));
                    state->roomText = buffer;
                    state->outcome = RoomPromptState::Outcome::Join;
                    state->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                case kCancelButtonId:
                    state->outcome = RoomPromptState::Outcome::Cancelled;
                    state->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_DESTROY: {
            auto* state = roomStateFor(hwnd);
            if (state) {
                state->done = true;
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// Modal relative to `owner` (disabled by the caller around this call) - runs its own
// nested message loop exactly like show()'s own, just for the smaller popup window.
// Returns std::nullopt if the user cancels or closes the popup without choosing.
std::optional<RoomPopupResult> showRoomPopup(HINSTANCE hInstance, HWND owner) {
    RoomPromptState state;

    const char* className = "KungFuChessRoomPrompt";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = RoomWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc)) {
        std::cerr << "[UsernamePrompt] Room popup RegisterClassA failed, error " << GetLastError() << std::endl;
        return std::nullopt;
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Room", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                 CW_USEDEFAULT, CW_USEDEFAULT, RenderConfig::kRoomPromptWindowWidth,
                                 RenderConfig::kRoomPromptWindowHeight, owner, nullptr, hInstance, &state);
    if (!hwnd) {
        std::cerr << "[UsernamePrompt] Room popup CreateWindowExA failed, error " << GetLastError() << std::endl;
        UnregisterClassA(className, hInstance);
        return std::nullopt;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassA(className, hInstance);

    if (state.outcome == RoomPromptState::Outcome::Create) {
        return RoomPopupResult{true, ""};
    }
    if (state.outcome == RoomPromptState::Outcome::Join) {
        return RoomPopupResult{false, state.roomText};
    }
    return std::nullopt;
}

// --- Main window: username + password + Play (quick match) + Room (create/join) -------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<PromptState*>(cs->lpCreateParams);

            CreateWindowExA(0, "STATIC", "Username:", WS_CHILD | WS_VISIBLE, RenderConfig::kUsernamePromptLabelX,
                             RenderConfig::kUsernamePromptLabelY, RenderConfig::kUsernamePromptLabelWidth,
                             RenderConfig::kUsernamePromptLabelHeight, hwnd, nullptr, cs->hInstance, nullptr);
            state->edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           RenderConfig::kUsernamePromptEditX, RenderConfig::kUsernamePromptEditY,
                                           RenderConfig::kUsernamePromptEditWidth, RenderConfig::kUsernamePromptEditHeight,
                                           hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)), cs->hInstance,
                                           nullptr);

            CreateWindowExA(0, "STATIC", "Password:", WS_CHILD | WS_VISIBLE, RenderConfig::kUsernamePromptLabelX,
                             RenderConfig::kUsernamePromptPasswordLabelY, RenderConfig::kUsernamePromptLabelWidth,
                             RenderConfig::kUsernamePromptLabelHeight, hwnd, nullptr, cs->hInstance, nullptr);
            // ES_PASSWORD masks every typed character as '*' - the one thing that needs to
            // be explicit here, everything else is identical to the username EDIT above.
            state->passwordEdit =
                CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                                 RenderConfig::kUsernamePromptEditX, RenderConfig::kUsernamePromptPasswordEditY,
                                 RenderConfig::kUsernamePromptEditWidth, RenderConfig::kUsernamePromptEditHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPasswordEditId)), cs->hInstance,
                                 nullptr);

            CreateWindowExA(0, "STATIC", state->errorMessage.c_str(), WS_CHILD | WS_VISIBLE,
                             RenderConfig::kUsernamePromptLabelX, RenderConfig::kUsernamePromptErrorLabelY,
                             RenderConfig::kUsernamePromptLabelWidth, RenderConfig::kUsernamePromptErrorLabelHeight,
                             hwnd, nullptr, cs->hInstance, nullptr);

            CreateWindowExA(0, "BUTTON", "Play", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                             RenderConfig::kUsernamePromptPlayButtonX, RenderConfig::kUsernamePromptButtonY,
                             RenderConfig::kUsernamePromptButtonWidth, RenderConfig::kUsernamePromptButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayButtonId)), cs->hInstance, nullptr);
            CreateWindowExA(0, "BUTTON", "Room", WS_CHILD | WS_VISIBLE,
                             RenderConfig::kUsernamePromptRoomButtonX, RenderConfig::kUsernamePromptButtonY,
                             RenderConfig::kUsernamePromptButtonWidth, RenderConfig::kUsernamePromptButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRoomButtonId)), cs->hInstance, nullptr);
            SetFocus(state->edit);
            return 0;
        }
        case WM_COMMAND: {
            auto* state = stateFor(hwnd);
            if (!state) {
                break;
            }

            if (LOWORD(wParam) == kPlayButtonId || LOWORD(wParam) == kRoomButtonId) {
                char userBuffer[256] = {};
                GetWindowTextA(state->edit, userBuffer, sizeof(userBuffer));
                const std::string username = sanitize(userBuffer, kMaxUsernameLength);

                char passBuffer[256] = {};
                GetWindowTextA(state->passwordEdit, passBuffer, sizeof(passBuffer));
                const std::string password = sanitize(passBuffer, kMaxPasswordLength);

                if (LOWORD(wParam) == kPlayButtonId) {
                    state->result = JoinInfo{username, password, JoinInfo::Mode::QuickMatch, ""};
                    state->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }

                // Room button.
                EnableWindow(hwnd, FALSE);
                const auto choice = showRoomPopup(GetModuleHandle(nullptr), hwnd);
                EnableWindow(hwnd, TRUE);

                if (!choice.has_value()) {
                    SetFocus(state->edit);
                    return 0;  // Cancelled/closed - main window stays open, untouched
                }

                if (choice->create) {
                    state->result = JoinInfo{username, password, JoinInfo::Mode::CreateRoom, ""};
                } else {
                    const std::string room = sanitize(choice->typedRoom, kMaxRoomLength);
                    if (room.empty()) {
                        SetFocus(state->edit);
                        return 0;  // Join with an empty id is a no-op - stay on the main window
                    }
                    state->result = JoinInfo{username, password, JoinInfo::Mode::JoinRoom, room};
                }
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
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

// Used only if the native window can't be created at all (see show()) - so the app
// always has *some* way to get a username instead of silently hanging forever with no
// visible window and no explanation.
std::optional<JoinInfo> promptOnConsole() {
    std::cout << "(Couldn't open a native window here - type your choices instead.)\n"
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

    // No portable console-masking without extra platform code - accepted as a minor gap
    // in an already-degraded last-resort path (the native window is always tried first).
    std::cout << "Password (visible - this fallback has no input masking): " << std::flush;
    std::string passwordLine;
    if (!std::getline(std::cin, passwordLine)) {
        return std::nullopt;
    }
    const std::string password = sanitize(passwordLine, kMaxPasswordLength);

    std::cout << "Room id to create/join, \"create\" for a fresh room, or blank for quick match: " << std::flush;
    std::string roomLine;
    if (!std::getline(std::cin, roomLine)) {
        return std::nullopt;
    }
    const std::string room = sanitize(roomLine, kMaxRoomLength);
    if (room.empty()) {
        return JoinInfo{username, password, JoinInfo::Mode::QuickMatch, ""};
    }
    if (room == "create") {
        return JoinInfo{username, password, JoinInfo::Mode::CreateRoom, ""};
    }
    return JoinInfo{username, password, JoinInfo::Mode::JoinRoom, room};
}

}  // namespace

std::optional<JoinInfo> UsernamePrompt::show(const std::string& errorMessage) {
    PromptState state;
    state.errorMessage = errorMessage;

    const char* className = "KungFuChessUsernamePrompt";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    if (!RegisterClassA(&wc)) {
        std::cerr << "[UsernamePrompt] RegisterClassA failed, error " << GetLastError() << std::endl;
        return promptOnConsole();
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Join",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
                                 CW_USEDEFAULT, RenderConfig::kUsernamePromptWindowWidth,
                                 RenderConfig::kUsernamePromptWindowHeight, nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) {
        std::cerr << "[UsernamePrompt] CreateWindowExA failed, error " << GetLastError() << std::endl;
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

}  // namespace kungfu
