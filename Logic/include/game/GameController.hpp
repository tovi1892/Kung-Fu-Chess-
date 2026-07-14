#pragma once

#include <memory>
#include <optional>

#include "common/Position.hpp"
#include "game/Game.hpp"
#include "game/IGameInputAdapter.hpp"

namespace kungfu {

class GameController final : public IGameInputTarget {
public:
    explicit GameController(std::shared_ptr<Game> game);

    void attachGame(std::shared_ptr<Game> game);
    std::shared_ptr<Game> game() const;

    bool handleCellClick(int row, int col);
    void handleTimePassed(int ms);

    bool isRunning() const override;
    bool isFriendlyPieceAt(const Position& pos) const override;
    bool selectPiece(const Position& pos) override;
    bool requestMove(const Position& from, const Position& to) override;
    bool requestJump(const Position& pos) override;
    bool hasSelection() const override;
    std::optional<Position> selectedPosition() const override;
    bool isPositionInBounds(const Position& pos) const override;

private:
    std::shared_ptr<Game> game_;
    std::optional<Position> selectedPosition_;
};

}  // namespace kungfu
