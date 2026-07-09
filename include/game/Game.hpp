#pragma once

#include <memory>
#include <optional>
#include <iostream>
#include "board/IBoard.hpp"
#include "collision/CollisionSystem.hpp"
#include "common/GameState.hpp"
#include "common/Position.hpp"
#include "movement/MovementSystem.hpp"
#include "pieces/Piece.hpp"
#include "rules/IRuleEngine.hpp"

namespace kungfu {

// מבנה פנימי לניהול תנועות מושהות בזמן אמת
struct PendingMove {
    Position from;
    Position to;
    int arrivalTimeMs;
};

class Game {
public:
    Game();
    explicit Game(std::shared_ptr<IBoard> board);
    Game(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine);

    void start();
    void stop();
    bool isRunning() const;
    bool isFinished() const;

    // תנועה מיידית (עבור מנוע הליבה ובדיקות יחידה)
    bool tryMove(const Position& from, const Position& to);

    // --- דרישות שלב ב' (Iteration 2) ---
    void click(int x, int y);
    void wait(int ms);
    void printBoard(std::ostream& out) const;

    // --- דרישות שלב ג' והלאה (Airborne / Jumps) ---
    bool tryJump(const Position& cell);
    void resolveJump(const Position& cell);
    bool handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom);

private:
    std::string getPieceToken(const PiecePtr& piece) const;

    GameState state_;
    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    std::shared_ptr<CollisionSystem> collisionSystem_;
    MovementSystem movementSystem_;

    // משתני ניהול זמן וממשק אינטראקטיבי
    std::optional<Position> selectedPosition_;
    std::optional<PendingMove> pendingMove_;
    int currentTimeMs_ = 0;
};

}  // namespace kungfu