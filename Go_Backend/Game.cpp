#include "Game.hpp"

#include <fstream>
#include <iostream>
#include <utility>
#include <sstream>

#include <boost/date_time/gregorian/gregorian.hpp>

#include "GoSetupUtil.h"
#include "SgGameWriter.h"
#include "SgProp.h"
#include "GoModBoard.h"

namespace GoBackend {
Game::Game()
    : _go_game(),
      _game_finished(false),
      _while_capturing(false),
      _allow_placing_handicap(false)
{}

bool Game::validSetup(const GoSetup& setup) const {
    if (!allValidPoints(setup.m_stones[SG_BLACK])
        || !allValidPoints(setup.m_stones[SG_WHITE]))
        return false;
    return true;
}

bool Game::allValidPoints(const SgPointSet& stones) const {
    auto& board = getBoard();

    // check all stones
    for (auto iter = SgSetIterator(stones); iter; ++iter) {
        auto point = *iter;

        if (!board.IsValidPoint(point))
            return false;
    }

    return true;
}

bool Game::init(int size, GoSetup setup, GoRules rules) {
    // assert valid board size
    assert(size < 20 && size > 1);
    assert(rules.Handicap() == 0);

    _go_game.Init(size, rules);

    // check if setup contains only valid stones!
    if (!validSetup(setup)) {
        std::cout << __TIMESTAMP__ << "[" << __FUNCTION__ << "] " << " GoSetup contains invalid stones! Skipping..." << std::endl;
        return false;
    }

    if (setup.m_stones[SG_WHITE].Size() == 0) {
        // no or only black stones, consider them to be handicap stones
        _allow_placing_handicap = true;
        placeHandicap(setup);
    }
    else {
        _allow_placing_handicap = false;
        // a SgBWArray<SgPointSet> is essentially the same as a SgBWSet
        // but SetupPosition wants a SbBWArray...
        _go_game.SetupPosition(SgBWArray<SgPointSet>(setup.m_stones[SG_BLACK], setup.m_stones[SG_WHITE]));
    }

    _game_finished = false;
    _while_capturing = false;

    return true;
}

const GoBoard& Game::getBoard() const {
    return _go_game.Board();
}


UpadateResult Game::update(GoSetup setup) {
    // check if setup contains only valid stones!
    if (!validSetup(setup)) {
        std::cout << __TIMESTAMP__ << " [" << __FUNCTION__ << "] " << " GoSetup contains invalid stones! Skipping..." << std::endl;
        return UpadateResult::Illegal;
    }

    // get new and current stones
    auto new_blacks = setup.m_stones[SG_BLACK];
    auto new_whites = setup.m_stones[SG_WHITE];

    auto current_blacks = getBoard().All(SG_BLACK);
    auto current_whites = getBoard().All(SG_WHITE);

    // extract new played stones with respect to the current board
    auto added_blacks = new_blacks - current_blacks;
    auto added_whites = new_whites - current_whites;

    auto removed_blacks = current_blacks - new_blacks;
    auto removed_whites = current_whites - new_whites;


    // handicap
    if (_allow_placing_handicap) {
        assert(_while_capturing == false);

        if (noWhites(current_whites, new_whites)) {
            placeHandicap(setup);
            return UpadateResult::Legal;
        }
        else {
            // white stone has been added, no handicap stones from now on
            _allow_placing_handicap = false;
        }
    }


    if (_while_capturing) {
        assert(getBoard().CapturingMove());
        return updateWhileCapturing(setup);
    }
    else {
        return updateNormal(added_blacks, added_whites, removed_blacks, removed_whites);
    }
}

bool Game::noWhites(SgPointSet current_whites, SgPointSet new_whites) {
    return current_whites.IsEmpty() && new_whites.IsEmpty();
}


UpadateResult Game::updateNormal(SgPointSet added_blacks, SgPointSet added_whites, SgPointSet removed_blacks, SgPointSet removed_whites) {
    assert(_while_capturing == false);

    if (added_blacks.IsEmpty() && added_whites.IsEmpty()) {
        if (removed_blacks.IsEmpty() && removed_whites.IsEmpty()) {
            return UpadateResult::Legal;
        }
        else {
            // just removing stones from the board is illegal
            // capturing is not covered here
            return UpadateResult::Illegal;
        }
    }
    else if (added_blacks.Size() == 1 && added_whites.Size() == 0) {
        // black move
        return playMove(added_blacks.PointOf(), SG_BLACK, removed_blacks, removed_whites);
    }
    else if (added_blacks.Size() == 0 && added_whites.Size() == 1) {
        // white move
        return playMove(added_whites.PointOf(), SG_WHITE, removed_whites, removed_blacks);
    }
    else {
        // more than a single stone added
        return UpadateResult::Illegal;
    }
}

UpadateResult Game::updateWhileCapturing(GoSetup new_setup) {
    auto board_setup = GoSetupUtil::CurrentPosSetup(getBoard());

    // the player of the setup is ignored
    new_setup.m_player = board_setup.m_player;

    if (new_setup == board_setup) {
        // all stones that are to capture have been removed from the board
        _while_capturing = false;
        return UpadateResult::Legal;
    }
    else {
        // real life board dosn't match internal state
        return UpadateResult::Illegal;
    }
}

UpadateResult Game::playMove(SgPoint point, SgBlackWhite player, SgPointSet removed_of_player, SgPointSet removed_of_opponent) {
    if (getBoard().ToPlay() != player || !getBoard().IsLegal(point, player)) {
        // illegal move by game rules
        return UpadateResult::Illegal;
    }
    
    SgPointSet captured_stones = possibleCapturedStones(getBoard(), point);
    if (captured_stones.IsEmpty()) {
        // no capture

        if (removed_of_player.IsEmpty() && removed_of_opponent.IsEmpty()) {
            // completely valid move
            _go_game.AddMove(point, player);
            return UpadateResult::Legal;
        }
        else {
            // played a valid move, but stones have been removed
            return UpadateResult::Illegal;
        }
    }
    else {
        // captured some stones

        if (!removed_of_player.IsEmpty()) {
            // only the enemy's stones can be captured
            return UpadateResult::Illegal;
        }

        if (removed_of_opponent == captured_stones) {
            // all stones that are to capture have already been removed
            _go_game.AddMove(point, player);
            return UpadateResult::Legal;
        }
        else if (removed_of_opponent.IsEmpty() || removed_of_opponent.SubsetOf(captured_stones)) {
            // legal capturing move
            _go_game.AddMove(point, player);

            // some stones may have already been removed after playing the move,
            // but there are still stones left to be removed, tell the user to remove them as well
            _while_capturing = true;
            return UpadateResult::Illegal;
        }
        else {
            // stones that are not beeing captured have been removed
            return UpadateResult::Illegal;
        }
    }
}

SgPointSet Game::possibleCapturedStones(const GoBoard& const_board, SgPoint move) {
    // makes the const_board modifiable, but asserts that the state has been restored when getting deleted
    GoModBoard mod_board(const_board);
    GoBoard& board = mod_board.Board();

    assert(board.IsLegal(move));
    // try playing to see if this move captured any stones
    board.Play(move);
    const GoPointList captured_list = mod_board.Board().CapturedStones();
    board.Undo();
    
    // convert to other datatype
    SgPointSet captured_set;
    for (auto it = GoPointList::Iterator(captured_list); it; ++it) {
        captured_set.Include(*it);
    }
    return captured_set;
}

void Game::placeHandicap(GoSetup new_setup) {
    auto previous_blacks = getBoard().All(SG_BLACK);
    auto blacks = new_setup.m_stones[SG_BLACK];
    auto whites = new_setup.m_stones[SG_WHITE];
    assert(whites.IsEmpty());

    if (!blacks.IsEmpty() && blacks.Size() != previous_blacks.Size()) {
        // the GoGame class only allows adding handicap stones all at once
        // when getting a new handicap stone, the GoGame is therefore reset and 
        // and all handicap stones ar added anew

        // the PlaceHandicap() call also increases the handicap inside the rules of the board,
        // these would then be placed automatically by the Init() call, we don't want that
        GoRules rules = getBoard().Rules();
        rules.SetHandicap(0);
        _go_game.Init(getBoard().Size(), rules);

        if (blacks.Size() == 1) {
            // only a single black stone played and is therefore not a handicap stone
            // play the black stone just like a regular move
            assert(getBoard().IsLegal(blacks.PointOf(), SG_BLACK));
            _go_game.AddMove(blacks.PointOf(), SG_BLACK);
        }
        else {
            SgVector<SgPoint> handicap_stones;
            blacks.ToVector(&handicap_stones);
            _go_game.PlaceHandicap(handicap_stones);
        }
    }
}



bool Game::saveGame(string file_path, string name_black, string name_white, string game_name) {
    using boost::gregorian::day_clock;
    using boost::gregorian::to_simple_string;

    std::ofstream file(file_path.c_str());
    if (!file.is_open()) {
        return false;
    }
    
    if (!name_black.empty())
        _go_game.UpdatePlayerName(SG_BLACK, name_black);
    if (!name_white.empty())
        _go_game.UpdatePlayerName(SG_WHITE, name_white);
    if (!game_name.empty())
        _go_game.UpdateGameName(game_name);

    // current date
    string date = to_simple_string(day_clock::local_day());
    _go_game.UpdateDate(date);

    SgGameWriter writer(file);

    bool all_props = true; // all properties like player names and game name
    int file_format = 0; // default file format
    int game_number = SG_PROPPOINTFMT_GO; // the game of go
    int default_size = 19; // default boardsize, never actually relevant as _go_game.Init gets always called
    writer.WriteGame(_go_game.Root(), all_props, file_format, game_number, default_size);

    return true;
}

std::string Game::finishGame() {
    pass();
    pass();

    return getResult();
}

void Game::pass() {
    _go_game.AddMove(SG_PASS, getBoard().ToPlay());

    // update result if the game ended with the second pass
    if (_go_game.EndOfGame()) {
        // get score and update result
        float score = FLT_MIN;
        auto score_successful = GoBoardUtil::ScorePosition(getBoard(), SgPointSet(), score);

        if (score_successful) {
            if (score == 0) {
                // this is a draw
                _go_game.UpdateResult("0");
            }
            else {
                // convert float score to string
                std::ostringstream stream;
                stream.precision(3);
                stream << std::abs(score);

                // a negative score means that black lost
                // sgf: RE: Result: result, usually in the format "B+3.5" (black wins by 3.5 moku). 
                auto result = std::string("") + (score < 0 ? "W" : "B") + "+" + stream.str();
                _go_game.UpdateResult(result);
            }
        }
        else {
            _go_game.UpdateResult("Couldn't score the board.");
        }

        _game_finished = true;
    }
}

void Game::resign() {
    auto current_player = getBoard().ToPlay();

    // adding a resign comment in the sgf structure
    _go_game.AddResignNode(current_player);

    // sgf: RE: Result: result, usually in the format "B+R" (Black wins by resign)
    auto result = std::string("") + (current_player == SG_BLACK ? "W" : "B") + "+R";   
    _go_game.UpdateResult(result);

    _game_finished = true;
}

std::string Game::getResult() const {
    if (_go_game.GetResult() != "")
        return _go_game.GetResult();
    else
        return "";
}

bool Game::hasEnded() const {
    return _game_finished;
}

} // 
