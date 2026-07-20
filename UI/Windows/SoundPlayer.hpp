#pragma once

#include <string>

namespace kungfu {

// The one place allowed to touch the Windows sound API directly (mirrors how Img is the
// one place allowed to touch cv::). Wraps PlaySoundW (winmm.lib) - a native OS API, not
// a new third-party dependency.
class SoundPlayer {
public:
    // Plays 'path' asynchronously. Silently does nothing if the file doesn't exist yet -
    // so real .wav files can be dropped into UI/assets/sounds/ later with zero code
    // changes, and a missing asset never interrupts gameplay.
    void play(const std::string& path) const;
};

}  // namespace kungfu
