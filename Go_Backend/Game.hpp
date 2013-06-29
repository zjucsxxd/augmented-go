// Copyright (c) 2013 augmented-go team
// See the file LICENSE for full license and copying terms.
#pragma once

#include "GoGame.h"
#include <string>


namespace GoBackend {
using std::string;

// possible states of the Game
enum class State {
    Valid,

    // in this state, the user has to restore the last valid state of the board
    // getBoard() will contain the last valid state
    Invalid,

    // basically the same as Invalid, but can only be reached through a capturing move
    WhileCapturing
};

/**
 * Basic game class to play and keep track of a game of go. One instance handles one
 * game of go. 
 * The board locations begin at (1,1) and go up to max. (19,19), depending on the
 * board size. These points are represented by the fuego type 'SgPoint'.
 * The fuego type 'GoSetup' is used for updating the board. GoSetup uses
 * 'SgPoints' to add stones to such a setup. The game then automatically tries to read
 * a move out of the current and updated board.
 *
 * The rules are:
 *   Handicap: 0
 *   Scoring: Japanese
 *   Komi: 6.5
 *   2 Passes end a game
 *   
 * Usage Example:
 *   Game game;
 *   GoSetup setup;
 *   setup.AddWhite(SgPointUtil::Pt(1, 1));
 *
 *   game.init(9, setup);
 *
 *   setup.AddBlack(SgPointUtil::Pt(2, 2));
 *   game.update(setup);
 */
class Game {

public:
    Game();

    /**
     * @brief       Initializes the game board with given size and setup (starting positions)
     * @param[in]   size    board size (size x size)
     * @param[in]   setup   initial board setup
     * @returns     false - the setup contains invalid stones\n
     *              true  - init successful
     */
    bool init(int size, GoSetup setup = GoSetup());

    /**
     * @brief       Updates the game with the given setup. Tries to extract a valid move from the setup.
     *              Modifies the internal game state if no valid move was found.
     *              Note: Ignores current player information in setup. Also ignores setups with invalid stones!
     *              These will be silently skipped.
     * @param[in]   setup   new board setup
     */
    void update(GoSetup setup);

    /**
     * @brief       Get current board information
     * @returns     reference to current board instance
     */
    const GoBoard& getBoard() const;

    /**
     * @brief       Get current game state
     * @returns     current game state
     */
    State getState() const;

    /**
     * @brief        Writes the current game state to a sgf file.
     *               Names for players and game are only added to the file if not empty.
     *               Also includes the current date.
     * @returns      false if the file could not be opened
     */
    bool saveGame(string file_path, string name_black = "", string name_white = "", string game_name = "");

    /**
     * @brief        Convenience function to quickly finish a game. Plays a pass for each player and
     *               returns the result (see Game::getResult)
     */
    std::string finishGame();

    /**
     * @brief        Plays a 'pass' for the current player.
     */
    void pass();

    /**
     * @brief        Current player resigns and the game ends.
     */
    void resign();

    /**
     * @brief        Returns the result of the game in a standard way,
     *               for example W+1.5 (White wins by 1.5 moku)
     *               or B+R (Black wins by resign)
     *               or 0 for a drawn game
     *               If the game was not ended properly, this returns an empty string.
     *               Also doesn't consider any dead stones!
     */
    std::string getResult() const;

private:
    // Not implemented
    Game(const Game&);
    Game& operator=(const Game&);

private:
    // these functions are called on update with the current state matching
    // they may change the state
    void updateValid(SgPointSet added_blacks, SgPointSet added_whites, 
                     SgPointSet removed_blacks, SgPointSet removed_whites);
    //void updateWhileCapturing(SgPointSet added_blacks, SgPointSet added_whites, 
    //                          SgPointSet removed_blacks, SgPointSet removed_whites);
    void updateInvalid(GoSetup new_setup);

    bool validSetup(const GoSetup& setup) const;
    bool allValidPoints(const SgPointSet& stones) const;

private:
    GoGame _go_game;
    State  _current_state;
};

}