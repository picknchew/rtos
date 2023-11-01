#include "rps_server.h"

#include <stdbool.h>

#include "rpi.h"
#include "syscall.h"
#include "task.h"
#include "user/server/name_server.h"

enum RPSRequestType { RPS_REQUEST_SIGNUP, RPS_REQUEST_PLAY, RPS_REQUEST_QUIT };

struct RPSRequest {
  enum RPSRequestType type;
  enum RPSMove move;
};

struct RPSPlayerStatus {
  int opponent_tid;
  enum RPSMove chosen_move;
  bool left_game;
};

static enum RPSRoundState get_round_state(enum RPSMove move, enum RPSMove opp_move) {
  if (move == opp_move) {
    return RPS_ROUND_STATE_DRAW;
  }

  if (((move == RPS_MOVE_ROCK) && (opp_move == RPS_MOVE_SCISSORS)) ||
      ((move == RPS_MOVE_PAPER) && (opp_move == RPS_MOVE_ROCK)) ||
      ((move == RPS_MOVE_SCISSORS) && (opp_move == RPS_MOVE_PAPER))) {
    return RPS_ROUND_STATE_WIN;
  }

  return RPS_ROUND_STATE_LOSE;
}

static char *move_to_string(enum RPSMove move) {
  switch (move) {
    case RPS_MOVE_ROCK:
      return "rock";
    case RPS_MOVE_PAPER:
      return "paper";
    case RPS_MOVE_SCISSORS:
      return "scissors";
    case RPS_MOVE_NONE:
      return "none";
  }

  return "unknown";
}

static void player_status_reset(struct RPSPlayerStatus *status) {
  status->opponent_tid = -1;
  status->chosen_move = RPS_MOVE_NONE;
  status->left_game = false;
}

static struct RPSPlayerStatus players[TASKS_MAX] = {0};

static void players_init() {
  for (int i = 0; i < TASKS_MAX; ++i) {
    player_status_reset(&players[i]);
  }
}

static void leave_game(int tid) {
  struct RPSPlayerStatus *status = &players[tid];

  status->opponent_tid = -1;
  status->left_game = true;

  printf("rps_server: %d quit the game\r\n", tid);

  struct RPSResponse res = {.type = RPS_RESP_QUIT};
  Reply(tid, (char *) &res, sizeof(res));
}

void handle_play_request(struct RPSRequest *req, int req_tid) {
  struct RPSPlayerStatus *req_player_status = &players[req_tid];
  struct RPSPlayerStatus *opp_player_status = &players[req_player_status->opponent_tid];
  req_player_status->chosen_move = req->move;

  if (opp_player_status->left_game) {
    leave_game(req_tid);
    return;
  }

  printf("rps_server: %d played %s\r\n", req_tid, move_to_string(req->move));

  if (players[req_player_status->opponent_tid].chosen_move != RPS_MOVE_NONE) {
    struct RPSResponse req_response = {
        .type = RPS_RESP_OPP_MOVE, .opponent_move = opp_player_status->chosen_move};
    struct RPSResponse opp_response = {
        .type = RPS_RESP_OPP_MOVE, .opponent_move = req_player_status->chosen_move};
    // opponent already made move
    Reply(req_tid, (char *) &req_response, sizeof(req_response));
    Reply(req_player_status->opponent_tid, (char *) &opp_response, sizeof(opp_response));

    switch (get_round_state(req_player_status->chosen_move, opp_player_status->chosen_move)) {
      case RPS_ROUND_STATE_DRAW:
        printf(
            "rps_server: %d drawed against %d, %s vs %s\r\n",
            req_tid,
            req_player_status->opponent_tid,
            move_to_string(req_player_status->chosen_move),
            move_to_string(opp_player_status->chosen_move));
        break;
      case RPS_ROUND_STATE_LOSE:
        printf(
            "rps_server: %d won against %d, %s vs %s\r\n",
            req_player_status->opponent_tid,
            req_tid,
            move_to_string(opp_player_status->chosen_move),
            move_to_string(req_player_status->chosen_move));
        break;
      case RPS_ROUND_STATE_WIN:
        printf(
            "rps_server: %d won against %d, %s vs %s\r\n",
            req_tid,
            req_player_status->opponent_tid,
            move_to_string(req_player_status->chosen_move),
            move_to_string(opp_player_status->chosen_move));
        break;
    }

    // reset for next round
    req_player_status->chosen_move = RPS_MOVE_NONE;
    opp_player_status->chosen_move = RPS_MOVE_NONE;
  }
}

void handle_signup_request(int req_tid) {
  static int last_queued_player_tid = -1;
  struct RPSPlayerStatus *req_player_status = &players[req_tid];

  player_status_reset(req_player_status);

  printf("rps_server: %d signed up\r\n", req_tid);

  if (last_queued_player_tid != -1) {
    printf("rps_server: %d was matched up with %d\r\n", req_tid, last_queued_player_tid);

    // join a game with another player waiting
    req_player_status->opponent_tid = last_queued_player_tid;
    players[last_queued_player_tid].opponent_tid = req_tid;
    last_queued_player_tid = -1;

    struct RPSResponse response = {.type = RPS_RESP_GAME_START};

    Reply(req_tid, (char *) &response, sizeof(response));
    Reply(req_player_status->opponent_tid, (char *) &response, sizeof(response));

    return;
  }

  // wait for another player to join
  last_queued_player_tid = req_tid;
}

void handle_quit_request(int req_tid) {
  struct RPSPlayerStatus *req_player_status = &players[req_tid];
  struct RPSPlayerStatus *opp_player_status = &players[req_player_status->opponent_tid];

  // check if player has already sent request
  leave_game(req_tid);

  if (!opp_player_status->left_game && opp_player_status->chosen_move != RPS_MOVE_NONE) {
    // opponent has already made move, reply with disconnect them as well
    leave_game(req_player_status->opponent_tid);
  }
}

void rps_server_task() {
  players_init();
  RegisterAs("rps");
  printf("rps_server: started with id %d\r\n", MyTid());

  while (true) {
    int req_tid;
    struct RPSRequest req;

    Receive(&req_tid, (char *) &req, sizeof(req));

    switch (req.type) {
      case RPS_REQUEST_SIGNUP:
        handle_signup_request(req_tid);
        break;
      case RPS_REQUEST_PLAY:
        handle_play_request(&req, req_tid);
        break;
      case RPS_REQUEST_QUIT:
        handle_quit_request(req_tid);
        break;
      default:
        printf("rps_server: received invalid request!\r\n");
    }
  }
}

struct RPSResponse SignUp() {
  int server_tid = WhoIs("rps");
  struct RPSRequest req = {.type = RPS_REQUEST_SIGNUP};
  struct RPSResponse res;

  Send(server_tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));
  return res;
}

struct RPSResponse MakeMove(enum RPSMove move) {
  int server_tid = WhoIs("rps");
  struct RPSRequest req = {.type = RPS_REQUEST_PLAY, .move = move};
  struct RPSResponse res;

  Send(server_tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));
  return res;
}

struct RPSResponse QuitGame() {
  int server_tid = WhoIs("rps");
  struct RPSRequest req = {.type = RPS_REQUEST_QUIT};
  struct RPSResponse res;

  Send(server_tid, (const char *) &req, sizeof(req), (char *) &res, sizeof(res));
  return res;
}
