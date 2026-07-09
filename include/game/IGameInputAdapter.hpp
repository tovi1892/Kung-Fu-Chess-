#pragma once

#include <memory>
#include <optional>

namespace kungfu {

enum class InputType {
    None,
    Click,
    Wait
};

struct InputCommand {
    InputType type = InputType::None;
    // For Click
    int x = 0;
    int y = 0;
    // For Wait
    int ms = 0;
};

class IGameInputAdapter {
public:
    virtual ~IGameInputAdapter() = default;

    virtual void handleClick(int x, int y) = 0;

    // Return next input command if available, otherwise empty optional.
    virtual std::optional<InputCommand> nextCommand() = 0;
};

using IGameInputAdapterPtr = std::shared_ptr<IGameInputAdapter>;

} // namespace kungfu
