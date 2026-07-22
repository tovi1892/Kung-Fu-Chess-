#pragma once

#include <functional>
#include <string>

#include "RoomChoiceScreen.hpp"

namespace kungfu {

// The third and final pre-game screen (LoginScreen -> RoomChoiceScreen ->
// PlayConfirmScreen). Shows the room choice made on the previous screen and a Play button -
// clicking Play is what actually triggers the network join (see main.cpp); the game window
// only ever opens once that join resolves successfully, never before. A Back button returns
// to RoomChoiceScreen - only while no join is in flight (see show()).
class PlayConfirmScreen {
public:
    enum class Outcome { Play, Back, Cancelled };

    // Returned by the caller-supplied pollJoin callback below - deliberately generic (no
    // net::/RemoteGameProxy type appears here) so this class stays Network-agnostic, same
    // boundary LoginScreen/RoomChoiceScreen already keep.
    enum class JoinStatus { Pending, Succeeded, Failed };

    // Blocks until Back is clicked (Outcome::Back), the window is closed
    // (Outcome::Cancelled), or Play is clicked and the resulting join succeeds
    // (Outcome::Play). `choice` is shown read-only (e.g. "Quick Match" or "Join Room:
    // ABC123") - this class only displays it, RoomChoiceScreen already made the actual
    // choice.
    //
    // onPlay is called exactly once, the instant Play is clicked - main.cpp's hook to call
    // RemoteGameProxy::join(...) at that moment. Once onPlay fires, this switches to a
    // disabled "Connecting..."/"Searching for opponent..." display (both Play and Back
    // disabled - no backing out of an in-flight join) and calls pollJoin() repeatedly while
    // still pumping this window's own messages, until it stops returning Pending:
    // Succeeded resolves show() with Outcome::Play (the caller then opens the game window);
    // Failed shows the returned failure text as an error and reverts to the normal,
    // interactive display (Play/Back re-enabled) rather than exiting - the player can retry
    // Play or click Back to choose a different mode.
    static Outcome show(const RoomChoice& choice, const std::function<void()>& onPlay,
                        const std::function<JoinStatus(std::string& failureText)>& pollJoin);
};

}  // namespace kungfu
