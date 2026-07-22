#include "PlayConfirmScreen.hpp"

#include <windows.h>

#include "ChessTheme.hpp"
#include "OpenCV/RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kBackButtonId = 401;
constexpr int kPlayButtonId = 402;

std::string formatModeText(const RoomChoice& choice) {
    switch (choice.mode) {
        case RoomChoice::Mode::CreateRoom: return "Create a new room";
        case RoomChoice::Mode::JoinRoom:   return "Join room: " + choice.room;
        default:                          return "Quick Match";
    }
}

// Quick-match can take up to a minute (server-side matchmaking search); Create/Join
// resolve near-instantly (room creation, or a WELCOME for an existing one) - the wording
// reflects that difference rather than using one generic label for both.
std::string waitingLabelFor(const RoomChoice& choice) {
    return choice.mode == RoomChoice::Mode::QuickMatch ? "Searching for opponent..." : "Connecting...";
}

struct PlayConfirmState {
    const ChessTheme* theme = nullptr;
    std::string modeText;
    std::string waitingLabel;
    HWND errorLabel = nullptr;
    HWND backButton = nullptr;
    HWND playButton = nullptr;
    std::string playButtonText = "Play";
    bool waiting = false;  // true once Play has been clicked and onPlay has fired
    bool done = false;
    PlayConfirmScreen::Outcome outcome = PlayConfirmScreen::Outcome::Cancelled;
    const std::function<void()>* onPlay = nullptr;
};

PlayConfirmState* stateFor(HWND hwnd) {
    return reinterpret_cast<PlayConfirmState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK PlayConfirmWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<PlayConfirmState*>(cs->lpCreateParams);
            const ChessTheme& theme = *state->theme;

            HWND title = CreateWindowExA(0, "STATIC", "Ready to play?", WS_CHILD | WS_VISIBLE,
                                          RenderConfig::kPlayConfirmScreenTitleX, RenderConfig::kPlayConfirmScreenTitleY,
                                          RenderConfig::kPlayConfirmScreenTitleWidth,
                                          RenderConfig::kPlayConfirmScreenTitleHeight, hwnd, nullptr, cs->hInstance,
                                          nullptr);
            SendMessage(title, WM_SETFONT, reinterpret_cast<WPARAM>(theme.titleFont()), TRUE);

            HWND mode = CreateWindowExA(0, "STATIC", state->modeText.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
                                        RenderConfig::kPlayConfirmScreenModeX, RenderConfig::kPlayConfirmScreenModeY,
                                        RenderConfig::kPlayConfirmScreenModeWidth,
                                        RenderConfig::kPlayConfirmScreenModeHeight, hwnd, nullptr, cs->hInstance,
                                        nullptr);
            SendMessage(mode, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->errorLabel = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                                 RenderConfig::kPlayConfirmScreenModeX,
                                                 RenderConfig::kPlayConfirmScreenErrorLabelY,
                                                 RenderConfig::kPlayConfirmScreenModeWidth,
                                                 RenderConfig::kPlayConfirmScreenErrorLabelHeight, hwnd, nullptr,
                                                 cs->hInstance, nullptr);
            SendMessage(state->errorLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->backButton =
                CreateWindowExA(0, "BUTTON", "Back", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 RenderConfig::kPlayConfirmScreenBackButtonX, RenderConfig::kPlayConfirmScreenButtonY,
                                 RenderConfig::kPlayConfirmScreenBackButtonWidth,
                                 RenderConfig::kPlayConfirmScreenButtonHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kBackButtonId)), cs->hInstance, nullptr);
            state->playButton =
                CreateWindowExA(0, "BUTTON", "Play", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                 RenderConfig::kPlayConfirmScreenPlayButtonX, RenderConfig::kPlayConfirmScreenButtonY,
                                 RenderConfig::kPlayConfirmScreenPlayButtonWidth,
                                 RenderConfig::kPlayConfirmScreenButtonHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayButtonId)), cs->hInstance, nullptr);

            SetFocus(state->playButton);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);
            return reinterpret_cast<INT_PTR>(state->theme->paintStatic(hdc, control == state->errorLabel));
        }
        case WM_DRAWITEM: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            auto* dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            const bool enabled = !(dis->itemState & ODS_DISABLED);
            if (wParam == static_cast<WPARAM>(kPlayButtonId)) {
                state->theme->drawButton(dis->hDC, dis->rcItem, state->playButtonText, enabled);
                return TRUE;
            }
            if (wParam == static_cast<WPARAM>(kBackButtonId)) {
                // Always drawn in the muted brush, same "secondary action" convention as
                // RoomChoiceScreen's Log Out - genuinely disabled (not just cosmetically)
                // while a join is in flight, but never the visually "primary" rust color.
                state->theme->drawButton(dis->hDC, dis->rcItem, "Back", false);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            auto* state = stateFor(hwnd);
            if (!state) break;

            const int id = LOWORD(wParam);
            if (id == kBackButtonId) {
                if (state->waiting) break;  // disabled during a join - shouldn't fire, guard anyway
                state->outcome = PlayConfirmScreen::Outcome::Back;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == kPlayButtonId) {
                if (state->waiting) return 0;  // idempotent guard against a stray double-click
                state->waiting = true;
                state->playButtonText = state->waitingLabel;
                SetWindowTextA(state->errorLabel, "");
                EnableWindow(state->playButton, FALSE);
                EnableWindow(state->backButton, FALSE);
                InvalidateRect(state->playButton, nullptr, TRUE);
                InvalidateRect(state->backButton, nullptr, TRUE);
                (*state->onPlay)();
                return 0;
            }
            break;
        }
        case WM_DESTROY: {
            // No PostQuitMessage - see LoginScreen.cpp's WM_DESTROY comment. This loop
            // exits via state->done, and a WM_QUIT posted here would poison whatever window
            // (RoomChoiceScreen on Back, a fresh LoginScreen were this ever reached that way)
            // gets shown next.
            auto* state = stateFor(hwnd);
            if (state) {
                state->done = true;
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

}  // namespace

PlayConfirmScreen::Outcome PlayConfirmScreen::show(const RoomChoice& choice, const std::function<void()>& onPlay,
                                                    const std::function<JoinStatus(std::string&)>& pollJoin) {
    ChessTheme theme;
    PlayConfirmState state;
    state.theme = &theme;
    state.modeText = formatModeText(choice);
    state.waitingLabel = waitingLabelFor(choice);
    state.onPlay = &onPlay;

    const char* className = "KungFuChessPlayConfirmScreen";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = PlayConfirmWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme.backgroundBrush();

    if (!RegisterClassA(&wc)) {
        return Outcome::Cancelled;
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Ready?",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                 RenderConfig::kPlayConfirmScreenWindowWidth,
                                 RenderConfig::kPlayConfirmScreenWindowHeight, nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) {
        UnregisterClassA(className, wc.hInstance);
        return Outcome::Cancelled;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    MSG msg;
    while (!state.done) {
        if (state.waiting) {
            // Can't block on GetMessage here - pollJoin() needs calling regularly while a
            // join is in flight, so messages are pumped non-blockingly instead.
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (state.done) break;
            }
            if (state.done) break;

            std::string failureText;
            const auto status = pollJoin(failureText);
            if (status == JoinStatus::Succeeded) {
                state.outcome = Outcome::Play;
                state.done = true;
                DestroyWindow(hwnd);
                break;
            }
            if (status == JoinStatus::Failed) {
                state.waiting = false;
                state.playButtonText = "Play";
                SetWindowTextA(state.errorLabel, failureText.c_str());
                EnableWindow(state.playButton, TRUE);
                EnableWindow(state.backButton, TRUE);
                InvalidateRect(state.playButton, nullptr, TRUE);
                InvalidateRect(state.backButton, nullptr, TRUE);
            }
            Sleep(50);
        } else {
            if (!GetMessage(&msg, nullptr, 0, 0)) {
                break;  // WM_QUIT retrieved - shouldn't happen (nothing here posts it), defensive only
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnregisterClassA(className, wc.hInstance);
    return state.outcome;
}

}  // namespace kungfu
