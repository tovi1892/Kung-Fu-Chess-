#pragma once

#include <memory>
#include <optional>
#include <iostream>
#include <vector>
#include "model/IBoard.hpp"
#include "collision/CollisionSystem.hpp"
#include "model/GameState.hpp"
#include "model/Position.hpp"
#include "movement/MovementSystem.hpp"
#include "model/pieces/Piece.hpp"
#include "rules/IRuleEngine.hpp"
#include "game/IGameInputAdapter.hpp"
#include "game/RealTimeArbiter.hpp" // הכללת ה-Arbiter החדש

// Forward-declare render struct from Core_Interfaces to avoid direct header dependency
namespace kungfu { struct RenderPiece; }

namespace kungfu {

class Game : public IGameInputTarget {
public:
    Game();
    explicit Game(std::shared_ptr<IBoard> board);
    Game(std::shared_ptr<IBoard> board, std::shared_ptr<IRuleEngine> ruleEngine);
    Game(std::shared_ptr<IBoard> board,
         std::shared_ptr<IRuleEngine> ruleEngine,
         IGameInputAdapterPtr inputAdapter);

    void start();
    void stop();
    bool isRunning() const;
    bool isFinished() const;

    bool tryMove(const Position& from, const Position& to);

    void click(int x, int y);
    void wait(int ms);
    void printBoard(std::ostream& out) const;

    bool tryJump(const Position& cell);
    void resolveJump(const Position& cell);
    bool handleArrivalAtAirbornCell(const Position& cell, const Position& arrivingFrom);

    std::shared_ptr<IBoard> getBoard() const;

    // Snapshot of renderable state for the UI (non-owning, lightweight)
    std::vector<kungfu::RenderPiece> getRenderState() const;

    bool selectPiece(const Position& pos);
    bool requestMove(const Position& from, const Position& to);
    bool requestJump(const Position& pos);
    bool hasSelection() const override;
    std::optional<Position> selectedPosition() const override;
    bool isFriendlyPieceAt(const Position& pos) const override;
    bool isPositionInBounds(const Position& pos) const override;
    int boardRows() const override;
    int boardCols() const override;

private:
    std::string getPieceToken(const Piece* piece) const;

    GameState state_;
    std::shared_ptr<IBoard> board_;
    std::shared_ptr<IRuleEngine> ruleEngine_;
    std::unique_ptr<CollisionSystem> collisionSystem_;
    MovementSystem movementSystem_;

    // הפרדת האחריות: ניהול הזמן והתנועות עבר ל-Arbiter
    std::unique_ptr<RealTimeArbiter> arbiter_; 
    
    const std::shared_ptr<IGameInputAdapter> inputAdapter_;
};

}  // namespace kungfu