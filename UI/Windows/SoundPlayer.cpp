#include "SoundPlayer.hpp"

#include <filesystem>

#include <windows.h>
#include <mmsystem.h>

namespace kungfu {

void SoundPlayer::play(const std::string& path) const {
    if (!std::filesystem::exists(path)) {
        return;
    }
    PlaySoundA(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
}

}  // namespace kungfu
