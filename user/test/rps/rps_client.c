#include "rps_client.h"

#include <stdbool.h>

#include "rpi.h"
#include "rps_server.h"
#include "syscall.h"
#include "user/server/name_server.h"

// https://www.javamex.com/tutorials/random_numbers/xorshift.shtml
static uint64_t rand() {
  static uint64_t seed = 77;

  seed ^= (seed << 21);
  seed ^= (seed >> 35);
  seed ^= (seed << 4);

  return seed;
}

void rps_client_task() {
  // queue up for game (blocks until opponent found)
  struct RPSResponse res = SignUp();

  bool game_ongoing = true;
  enum RPSMove last_move;

  while (game_ongoing) {
    int rand_num = rand() % 4;

    switch (res.type) {
      case RPS_RESP_OPP_MOVE:
      case RPS_RESP_GAME_START:
        // make next move
        // random number from 0 to 3 (inclusive)
        switch (rand_num) {
          case 0:
            last_move = RPS_MOVE_ROCK;
            break;
          case 1:
            last_move = RPS_MOVE_PAPER;
            break;
          case 2:
            last_move = RPS_MOVE_SCISSORS;
            break;
          case 3:
            res = QuitGame();
            continue;
        }

        res = MakeMove(last_move);
        break;
      case RPS_RESP_QUIT:
        game_ongoing = false;
        break;
    }
  }

  Exit();
}
