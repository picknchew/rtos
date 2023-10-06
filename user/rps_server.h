#pragma once

void rps_server_task();

enum RPSMove { RPS_MOVE_NONE, RPS_MOVE_ROCK, RPS_MOVE_PAPER, RPS_MOVE_SCISSORS };

enum RPSResponseType { RPS_RESP_GAME_START, RPS_RESP_OPP_MOVE, RPS_RESP_QUIT };

enum RPSRoundState { RPS_ROUND_STATE_DRAW, RPS_ROUND_STATE_WIN, RPS_ROUND_STATE_LOSE };

struct RPSResponse {
  enum RPSResponseType type;
  enum RPSMove opponent_move;
};

struct RPSResponse SignUp();
struct RPSResponse MakeMove(enum RPSMove move);
struct RPSResponse QuitGame();
