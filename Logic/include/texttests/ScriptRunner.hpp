#pragma once

#include <istream>
#include <ostream>

namespace kungfu {

// Command-script test harness. Drives the exact same path a real user/UI
// would: BoardParser for the "Board:" section,
// Controller for "click x y", GameEngine::wait for "wait ms", and
// BoardPrinter for "print board" - the DSL's only assertion mechanism. Never
// bypasses these to mutate the Board or the engine directly.
//
// Script format:
//   Board:
//   <rows of space-separated piece tokens, '.' for empty>
//   Commands:
//   click <x> <y>
//   wait <ms>
//   print board
class ScriptRunner {
public:
    // Parses and runs the full script from `in`, writing the output of every
    // "print board" command to `out`. Returns false on a malformed board.
    bool run(std::istream& in, std::ostream& out) const;
};

}  // namespace kungfu
