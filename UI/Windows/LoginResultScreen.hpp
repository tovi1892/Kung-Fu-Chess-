#pragma once

#include <string>

namespace kungfu {

// Shown once, right after LoginScreen::show() and the actual LOGIN round-trip both
// succeed: a brief "Welcome back, alice (1200)" / "Account created for alice (1200)"
// confirmation, so the player is told honestly whether their username just signed into an
// existing account or auto-registered a new one, rather than never finding out. Split out
// of LoginScreen (which only collects credentials) because this is a genuinely separate
// window/message loop that happens to run right after it - the two shared nothing but a
// file and the sanitize() helper LoginScreen alone still needs.
class LoginResultScreen {
public:
    // Blocks until dismissed (Continue button or closing the window - either way just
    // acknowledges the message, there's nothing else to decide here).
    static void show(const std::string& username, int rating, bool accountCreated);
};

}  // namespace kungfu
