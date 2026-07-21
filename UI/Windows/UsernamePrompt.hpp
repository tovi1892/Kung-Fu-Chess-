#pragma once

#include <optional>
#include <string>

namespace kungfu {

// What the player chose: quick-match a stranger (Play), create a fresh room (Room ->
// Create), or join a specific room by id (Room -> Join). Deliberately Network-agnostic -
// main.cpp maps this to net::JoinMode when constructing RemoteGameProxy, so this header
// (like the rest of UI/Windows/) stays free of any Network/ dependency.
struct JoinInfo {
    enum class Mode { QuickMatch, CreateRoom, JoinRoom };
    std::string username;
    std::string password;
    Mode mode = Mode::QuickMatch;
    std::string room;  // meaningful only when mode == JoinRoom
};

// A small native window: username + password fields (the password box is masked -
// ES_PASSWORD), a Play button (quick match), and a Room button (opens a second popup to
// create or join a specific room by id). The one place allowed to touch the Win32
// windowing API directly (mirrors Img for OpenCV, SoundPlayer for winmm) - OpenCV alone
// has no way to take keyboard text input, so this uses the OS's own windowing instead of
// building one keystroke at a time out of put_text.
class UsernamePrompt {
public:
    // Blocks until the user resolves the prompt via Play or Room->Create/Join (returns
    // the chosen JoinInfo - username/password/any room id are trimmed of whitespace and
    // capped to a sane length, since they become wire-protocol tokens), or closes the
    // window / cancels out entirely (returns std::nullopt). errorMessage, when non-empty,
    // is shown as a small error line under the fields - how the caller re-shows this
    // window after a LOGIN_FAIL or NO_OPPONENT to let the player retry.
    static std::optional<JoinInfo> show(const std::string& errorMessage = "");
};

}  // namespace kungfu
