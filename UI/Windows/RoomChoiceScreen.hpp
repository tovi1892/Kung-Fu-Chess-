#pragma once

#include <string>

namespace kungfu {

// What the player chose: quick-match a stranger, create a fresh room, or join a specific
// room by id. Deliberately Network-agnostic - main.cpp maps this to net::JoinMode when
// calling RemoteGameProxy::join(), so this header (like the rest of UI/Windows/) stays free
// of any Network/ dependency.
struct RoomChoice {
    enum class Mode { QuickMatch, CreateRoom, JoinRoom };
    Mode mode = Mode::QuickMatch;
    std::string room;  // meaningful only when mode == JoinRoom
};

// The second of the three pre-game screens (LoginScreen -> RoomChoiceScreen ->
// PlayConfirmScreen). Three mutually-exclusive choices (Quick Match / Create Room / Join
// Room, the last with its own room-id field), a Next button, and a Log Out button that
// returns to LoginScreen (see main.cpp - this tears down the current authenticated
// connection and starts a fresh one). Purely a local choice: this class never talks to the
// network itself - PlayConfirmScreen's own Play button is what actually sends JOIN, per the
// Login/Room/Play plan.
class RoomChoiceScreen {
public:
    enum class Outcome { Chosen, LogOut, Cancelled };

    struct Result {
        Outcome outcome = Outcome::Cancelled;
        RoomChoice choice;  // meaningful only when outcome == Chosen
    };

    // Blocks until Next is clicked with a valid choice (Outcome::Chosen + choice), Log Out
    // is clicked (Outcome::LogOut), or the window is closed (Outcome::Cancelled - main.cpp
    // treats this the same as closing the login window, i.e. quits the app). welcomeText is
    // shown at the top (e.g. "Signed in as alice (rating 1200)").
    static Result show(const std::string& welcomeText);
};

}  // namespace kungfu
