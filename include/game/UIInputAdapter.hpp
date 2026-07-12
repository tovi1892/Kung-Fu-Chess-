#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "common/Position.hpp"
#include "game/IGameInputAdapter.hpp"

namespace kungfu {

class BoardMapper {
public:
    std::optional<Position> mapToBoard(int x, int y, int maxRows, int maxCols) const;
};

class UIInputAdapter : public IGameInputAdapter {
public:
    explicit UIInputAdapter(IGameInputTarget& game);
    explicit UIInputAdapter(std::function<IGameInputTarget&()> targetProvider);

    void setBoardMapper(BoardMapper mapper);
    void handleClick(int x, int y) override;
    std::optional<InputCommand> nextCommand() override;

private:
    std::function<IGameInputTarget&()> targetProvider_;
    BoardMapper boardMapper_;
};

using UIInputAdapterPtr = std::shared_ptr<UIInputAdapter>;

}  // namespace kungfu
