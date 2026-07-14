#pragma once

#include <memory>
#include <optional>

#include "common/Position.hpp"

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

class IGameInputTarget {
public:
    virtual ~IGameInputTarget() = default;

    virtual bool isRunning() const = 0;
    virtual bool isFriendlyPieceAt(const Position& pos) const = 0;
    virtual bool selectPiece(const Position& pos) = 0;
    virtual bool requestMove(const Position& from, const Position& to) = 0;
    virtual bool requestJump(const Position& pos) = 0;
    virtual bool hasSelection() const = 0;
    virtual std::optional<Position> selectedPosition() const = 0;
    virtual bool isPositionInBounds(const Position& pos) const = 0;
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
