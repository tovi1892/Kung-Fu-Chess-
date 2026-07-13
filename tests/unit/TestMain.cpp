#include <cstdlib>
#include <iostream>

int PositionTests_main();
int PieceTests_main();
int BoardTests_main();
int RuleEngineTests_main();
int PieceTypesTests_main();
int MovementSystemTests_main();
int CollisionSystemTests_main();
int GameTests_main();
int PawnTests_main();
int JumpTests_main();
int ConfigAndBoundaryTests_main();
int UIInputAdapterTests_main();

int main() {
    const int results[] = {
        PositionTests_main(),
        PieceTests_main(),
        BoardTests_main(),
        RuleEngineTests_main(),
        PieceTypesTests_main(),
        MovementSystemTests_main(),
        CollisionSystemTests_main(),
        GameTests_main(),
        PawnTests_main(),
        JumpTests_main(),
        ConfigAndBoundaryTests_main(),
        UIInputAdapterTests_main()
    };

    for (int result : results) {
        std::cout << "suite result=" << result << "\n";
        if (result != EXIT_SUCCESS) {
            return result;
        }
    }

    return EXIT_SUCCESS;
}
