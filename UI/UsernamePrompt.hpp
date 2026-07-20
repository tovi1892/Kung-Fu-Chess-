#pragma once

#include <optional>
#include <string>

namespace kungfu {

// A small native window: a text field for a username and a Play button. The one place
// allowed to touch the Win32 windowing API directly (mirrors Img for OpenCV, SoundPlayer
// for winmm) - OpenCV alone has no way to take keyboard text input, so this uses the
// OS's own windowing instead of building one keystroke at a time out of put_text.
class UsernamePrompt {
public:
    // Blocks until the user clicks Play (returns the entered username - trimmed of
    // whitespace and capped to a sane length, since it becomes one token in the wire
    // protocol) or closes the window instead (returns std::nullopt).
    static std::optional<std::string> show();
};

}  // namespace kungfu
