#include "RoomChoiceScreen.hpp"

#include <cctype>

#include <windows.h>

#include "ChessTheme.hpp"
#include "OpenCV/RenderConfig.hpp"

namespace kungfu {

namespace {

constexpr int kQuickMatchRadioId = 301;
constexpr int kCreateRoomRadioId = 302;
constexpr int kJoinRoomRadioId = 303;
constexpr int kRoomEditId = 304;
constexpr int kNextButtonId = 305;
constexpr int kLogOutButtonId = 306;
constexpr size_t kMaxRoomLength = 20;

// The wire protocol is space-delimited text, so a room id can't contain whitespace - strip
// it rather than reject it. Same rule LoginScreen/PlayConfirmScreen apply to their own text
// fields.
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

struct RoomChoiceState {
    const ChessTheme* theme = nullptr;
    std::string welcomeText;
    HWND quickMatchRadio = nullptr;
    HWND createRoomRadio = nullptr;
    HWND joinRoomRadio = nullptr;
    HWND roomEdit = nullptr;
    bool done = false;
    RoomChoiceScreen::Result result;
};

RoomChoiceState* stateFor(HWND hwnd) {
    return reinterpret_cast<RoomChoiceState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

// The Join Room radio's room-id field only makes sense while Join Room is actually
// selected - greyed out (and un-focusable) otherwise, rather than sitting there always
// editable even when irrelevant to the currently selected mode.
void updateRoomEditEnabled(RoomChoiceState* state) {
    const bool joinSelected = SendMessage(state->joinRoomRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(state->roomEdit, joinSelected);
}

LRESULT CALLBACK RoomChoiceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            auto* state = reinterpret_cast<RoomChoiceState*>(cs->lpCreateParams);
            const ChessTheme& theme = *state->theme;

            HWND title = CreateWindowExA(0, "STATIC", "Choose how to play", WS_CHILD | WS_VISIBLE,
                                          RenderConfig::kRoomChoiceScreenTitleX, RenderConfig::kRoomChoiceScreenTitleY,
                                          RenderConfig::kRoomChoiceScreenTitleWidth,
                                          RenderConfig::kRoomChoiceScreenTitleHeight, hwnd, nullptr, cs->hInstance,
                                          nullptr);
            SendMessage(title, WM_SETFONT, reinterpret_cast<WPARAM>(theme.titleFont()), TRUE);

            HWND welcome = CreateWindowExA(0, "STATIC", state->welcomeText.c_str(), WS_CHILD | WS_VISIBLE,
                                            RenderConfig::kRoomChoiceScreenWelcomeX,
                                            RenderConfig::kRoomChoiceScreenWelcomeY,
                                            RenderConfig::kRoomChoiceScreenWelcomeWidth,
                                            RenderConfig::kRoomChoiceScreenWelcomeHeight, hwnd, nullptr, cs->hInstance,
                                            nullptr);
            SendMessage(welcome, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            // WS_GROUP on the first radio marks the start of this auto-exclusive group -
            // the three BS_AUTORADIOBUTTON controls created contiguously after it stay
            // mutually exclusive without any manual bookkeeping.
            state->quickMatchRadio =
                CreateWindowExA(0, "BUTTON", "Quick Match", WS_CHILD | WS_VISIBLE | WS_GROUP | BS_AUTORADIOBUTTON,
                                 RenderConfig::kRoomChoiceScreenRadioX, RenderConfig::kRoomChoiceScreenQuickMatchRadioY,
                                 RenderConfig::kRoomChoiceScreenRadioWidth, RenderConfig::kRoomChoiceScreenRadioHeight,
                                 hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kQuickMatchRadioId)),
                                 cs->hInstance, nullptr);
            state->createRoomRadio =
                CreateWindowExA(0, "BUTTON", "Create Room", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                 RenderConfig::kRoomChoiceScreenRadioX, RenderConfig::kRoomChoiceScreenCreateRoomRadioY,
                                 RenderConfig::kRoomChoiceScreenRadioWidth, RenderConfig::kRoomChoiceScreenRadioHeight,
                                 hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCreateRoomRadioId)),
                                 cs->hInstance, nullptr);
            state->joinRoomRadio =
                CreateWindowExA(0, "BUTTON", "Join Room", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                 RenderConfig::kRoomChoiceScreenRadioX, RenderConfig::kRoomChoiceScreenJoinRoomRadioY,
                                 RenderConfig::kRoomChoiceScreenRadioWidth, RenderConfig::kRoomChoiceScreenRadioHeight,
                                 hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kJoinRoomRadioId)), cs->hInstance,
                                 nullptr);
            for (HWND radio : {state->quickMatchRadio, state->createRoomRadio, state->joinRoomRadio}) {
                SendMessage(radio, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);
            }
            SendMessage(state->quickMatchRadio, BM_SETCHECK, BST_CHECKED, 0);

            HWND roomLabel = CreateWindowExA(0, "STATIC", "Room id:", WS_CHILD | WS_VISIBLE,
                                              RenderConfig::kRoomChoiceScreenRoomLabelX,
                                              RenderConfig::kRoomChoiceScreenRoomLabelY,
                                              RenderConfig::kRoomChoiceScreenRoomLabelWidth,
                                              RenderConfig::kRoomChoiceScreenRoomLabelHeight, hwnd, nullptr,
                                              cs->hInstance, nullptr);
            SendMessage(roomLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);

            state->roomEdit =
                CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 RenderConfig::kRoomChoiceScreenRoomEditX, RenderConfig::kRoomChoiceScreenRoomEditY,
                                 RenderConfig::kRoomChoiceScreenRoomEditWidth,
                                 RenderConfig::kRoomChoiceScreenRoomEditHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRoomEditId)), cs->hInstance, nullptr);
            SendMessage(state->roomEdit, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font()), TRUE);
            updateRoomEditEnabled(state);

            CreateWindowExA(0, "BUTTON", "Log Out", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             RenderConfig::kRoomChoiceScreenLogOutButtonX, RenderConfig::kRoomChoiceScreenButtonY,
                             RenderConfig::kRoomChoiceScreenLogOutButtonWidth,
                             RenderConfig::kRoomChoiceScreenButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLogOutButtonId)), cs->hInstance, nullptr);
            CreateWindowExA(0, "BUTTON", "Next", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             RenderConfig::kRoomChoiceScreenNextButtonX, RenderConfig::kRoomChoiceScreenButtonY,
                             RenderConfig::kRoomChoiceScreenNextButtonWidth,
                             RenderConfig::kRoomChoiceScreenButtonHeight, hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNextButtonId)), cs->hInstance, nullptr);

            SetFocus(state->quickMatchRadio);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            HDC hdc = reinterpret_cast<HDC>(wParam);
            return reinterpret_cast<INT_PTR>(state->theme->paintStatic(hdc));
        }
        case WM_CTLCOLOREDIT: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            return reinterpret_cast<INT_PTR>(state->theme->paintEdit(reinterpret_cast<HDC>(wParam)));
        }
        case WM_DRAWITEM: {
            auto* state = stateFor(hwnd);
            if (!state) break;
            auto* dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
            if (wParam == static_cast<WPARAM>(kNextButtonId)) {
                state->theme->drawButton(dis->hDC, dis->rcItem, "Next", true);
                return TRUE;
            }
            if (wParam == static_cast<WPARAM>(kLogOutButtonId)) {
                // Deliberately drawn in the "disabled" (muted) brush purely for a
                // secondary/de-emphasized look - the control itself stays fully enabled and
                // clickable, this only affects which of ChessTheme's two button colors it
                // paints with.
                state->theme->drawButton(dis->hDC, dis->rcItem, "Log Out", false);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            auto* state = stateFor(hwnd);
            if (!state) break;

            const int id = LOWORD(wParam);
            if (id == kQuickMatchRadioId || id == kCreateRoomRadioId || id == kJoinRoomRadioId) {
                updateRoomEditEnabled(state);
                return 0;
            }

            if (id == kLogOutButtonId) {
                state->result.outcome = RoomChoiceScreen::Outcome::LogOut;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }

            if (id == kNextButtonId) {
                RoomChoice choice;
                if (SendMessage(state->createRoomRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    choice.mode = RoomChoice::Mode::CreateRoom;
                } else if (SendMessage(state->joinRoomRadio, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    char buffer[256] = {};
                    GetWindowTextA(state->roomEdit, buffer, sizeof(buffer));
                    const std::string room = sanitize(buffer, kMaxRoomLength);
                    if (room.empty()) {
                        SetFocus(state->roomEdit);
                        return 0;  // Join with an empty id is a no-op - stay on this window
                    }
                    choice.mode = RoomChoice::Mode::JoinRoom;
                    choice.room = room;
                } else {
                    choice.mode = RoomChoice::Mode::QuickMatch;
                }

                state->result.outcome = RoomChoiceScreen::Outcome::Chosen;
                state->result.choice = choice;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_DESTROY: {
            // No PostQuitMessage - see LoginScreen.cpp's WM_DESTROY comment. This loop
            // exits via state->done, and a WM_QUIT posted here would sit unconsumed and
            // poison the next window's (PlayConfirmScreen, or a fresh LoginScreen after Log
            // Out) first GetMessage call instead.
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

RoomChoiceScreen::Result RoomChoiceScreen::show(const std::string& welcomeText) {
    ChessTheme theme;
    RoomChoiceState state;
    state.theme = &theme;
    state.welcomeText = welcomeText;

    const char* className = "KungFuChessRoomChoiceScreen";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = RoomChoiceWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = theme.backgroundBrush();

    if (!RegisterClassA(&wc)) {
        return Result{};  // Outcome::Cancelled by default - matches "closing the window"
    }

    HWND hwnd = CreateWindowExA(0, className, "Kung Fu Chess - Choose a Room",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                 RenderConfig::kRoomChoiceScreenWindowWidth, RenderConfig::kRoomChoiceScreenWindowHeight,
                                 nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) {
        UnregisterClassA(className, wc.hInstance);
        return Result{};
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
