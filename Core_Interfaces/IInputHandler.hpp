#pragma once

#include <memory>
#include <optional>

namespace kungfu {

// The other direction of the UI boundary: how a concrete view reports input
// back out, without needing to know what a "cell" or a "piece" is.
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
