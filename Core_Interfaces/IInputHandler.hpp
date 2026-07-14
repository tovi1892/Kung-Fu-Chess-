#pragma once

#include <memory>
#include <optional>

namespace kungfu {

class IInputHandler {
public:
	virtual ~IInputHandler() = default;
	// Called by UI when a mouse click occurs (window coords)
	virtual void handleClick(int x, int y) = 0;
	// Poll next input command for the game loop if any
	virtual std::optional<int> pollEvent() = 0;
};

using IInputHandlerPtr = std::shared_ptr<IInputHandler>;

} // namespace kungfu
