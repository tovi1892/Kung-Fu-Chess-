#pragma once

#include <memory>
#include <optional>

#include "common/Position.hpp"
#include "game/IGameInputAdapter.hpp"

namespace kungfu {

class Game;

class BoardMapper {
public:
    std::optional<Position> mapToBoard(int x, int y, int maxRows, int maxCols) const;
};

class UIInputAdapter : public IGameInputAdapter {
public:
    explicit UIInputAdapter(Game& game);

    void setBoardMapper(BoardMapper mapper);
    void handleClick(int x, int y) override;
    std::optional<InputCommand> nextCommand() override;

private:
    Game& game_;
    BoardMapper boardMapper_;
};

using UIInputAdapterPtr = std::shared_ptr<UIInputAdapter>;

}  // namespace kungfu
