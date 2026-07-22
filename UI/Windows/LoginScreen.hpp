#pragma once

#include <optional>
#include <string>
#include <utility>

namespace kungfu {

// The first of the three pre-game screens (LoginScreen -> RoomChoiceScreen ->
// PlayConfirmScreen). A small native window: username + password fields (the password box
// is masked - ES_PASSWORD) and a single Sign In button. Deliberately network-agnostic, same
// boundary the old combined UsernamePrompt already kept - this class only collects
// credentials, it never talks to RemoteGameProxy itself; main.cpp owns the one connection
// and performs the actual LOGIN round-trip after show() returns. One of the few places
// allowed to touch the Win32 windowing API directly (mirrors Img for OpenCV, SoundPlayer
// for winmm) - OpenCV alone has no way to take keyboard text input.
class LoginScreen {
public:
    // Blocks until Sign In is clicked (returns {username, password}, both trimmed of
    // whitespace and capped to a sane length since they become wire-protocol tokens) or the
    // window is closed (returns std::nullopt - main.cpp treats this as "quit the app", same
    // as the old UsernamePrompt's closed-window behavior). errorMessage, when non-empty, is
    // shown as a small red error line under the fields - how main.cpp re-shows this window
    // after a LOGIN_FAIL. prefillUsername pre-fills the username field (e.g. with what was
    // just typed, on a retry after a wrong password) - only the username, never the
    // password, which always starts blank.
    static std::optional<std::pair<std::string, std::string>> show(const std::string& errorMessage = "",
                                                                     const std::string& prefillUsername = "");

    // Shows a brief "Welcome back, alice (1200)" / "Account created for alice (1200)"
    // confirmation once LOGIN_OK has actually arrived - so the player is told honestly
    // whether their username just signed into an existing account or auto-registered a new
    // one, rather than never finding out. Blocks until dismissed (Continue button or
    // closing the window - either way just acknowledges the message, there's nothing else
    // to decide here).
    static void showResult(const std::string& username, int rating, bool accountCreated);
};

}  // namespace kungfu
