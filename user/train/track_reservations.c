#include "track_reservations.h"

#include <stdbool.h>
// #include <stdio.h>

#include "track_position.h"
#include "train_manager.h"
#include "user/server/name_server.h"
#include "user/terminal/terminal_task.h"
// static const char *color[6] = {"\033[0;31m",
//                                 "\033[0;32m",
//                                 "\033[0;33m",
//                                 "\033[0;34m",
//                                 "\033[0;35m",
//                                 "\033[0;36m"};

static struct Zone zones[ZONE_NUMBERS];

// // 20rows 50 cols

void zones_a_init() {
  // *(*(track + 2) + 1) = ".";
  for (int i = 0; i < ZONE_NUMBERS; i++) {
    zones[i].id = i;
    zones[i].reserved = false;
    zones[i].reservedby = -1;
  }
  zones[0].tracks[0] = 650;
  zones[0].len = 1;
  zones[0].color = red;
  zones[1].tracks[0] = 0;
  zones[1].tracks[1] = 1;
  zones[1].tracks[2] = 2;
  zones[1].tracks[3] = 3;
  zones[1].len = 4;
  zones[1].color = red;
  zones[2].tracks[0] = 100;
  zones[2].tracks[1] = 101;
  zones[2].tracks[2] = 102;
  zones[2].len = 3;
  zones[2].color = blue;
  zones[3].tracks[0] = 200;
  zones[3].len = 1;
  zones[3].color = red;
  zones[4].tracks[0] = 5;
  zones[4].tracks[1] = 6;
  zones[4].tracks[2] = 7;
  zones[4].tracks[3] = 8;
  zones[4].tracks[4] = 9;
  zones[4].tracks[5] = 10;
  zones[4].tracks[6] = 11;
  zones[4].tracks[7] = 12;
  zones[4].tracks[8] = 13;
  zones[4].tracks[9] = 5 + 50;
  zones[4].tracks[10] = 11 + 50;
  zones[4].tracks[11] = 100 + 4;
  zones[4].tracks[12] = 100 + 10;
  zones[4].tracks[13] = 150 + 3;
  zones[4].tracks[14] = 150 + 9;
  zones[4].tracks[22] = 200 + 2;
  zones[4].tracks[16] = 200 + 8;
  zones[4].tracks[17] = 150 + 12;
  zones[4].tracks[18] = 200 + 11;
  zones[4].tracks[19] = 250 + 7;
  zones[4].tracks[20] = 250 + 9;
  zones[4].tracks[21] = 300 + 6;
  zones[4].tracks[15] = 300 + 7;
  zones[4].len = 23;
  zones[4].color = green;
  for (int i = 0; i < 22; i++) {
    zones[5].tracks[i] = 15 + i;
  }
  zones[5].len = 22;
  zones[5].color = yellow;
  for (int i = 0; i < 8; i++) {
    zones[6].tracks[i] = 100 + 15 + i;
  }
  zones[6].tracks[8] = 150 + 22;
  zones[6].len = 9;
  zones[6].color = yellow;
  zones[7].tracks[0] = 400 + 6;
  zones[7].tracks[1] = 450 + 6;
  zones[7].len = 2;
  zones[7].color = yellow;
  for (int i = 0; i < 5; i++) {
    zones[8].tracks[i] = 38 + i;
  }
  zones[8].tracks[5] = 43 + 50;
  zones[8].len = 6;
  zones[8].color = blue;
  for (int i = 0; i < 7; i++) {
    zones[9].tracks[i] = 100 + 24 + i;
  }
  zones[9].len = 7;
  zones[9].color = blue;
  zones[10].tracks[0] = 250 + 24;
  zones[10].len = 1;
  zones[10].color = red;
  zones[11].tracks[0] = 350 + 26;
  zones[11].tracks[1] = 350 + 27;
  zones[11].tracks[2] = 350 + 28;
  zones[11].tracks[3] = 400 + 27;
  zones[11].tracks[4] = 450 + 27;
  zones[11].tracks[5] = 500 + 26;
  zones[11].tracks[6] = 500 + 27;
  zones[11].tracks[7] = 500 + 28;
  zones[11].len = 8;
  zones[11].color = purple;
  zones[12].tracks[0] = 250 + 30;
  zones[12].len = 1;
  zones[12].color = green;
  for (int i = 0; i < 5; i++) {
    zones[13].tracks[i] = 100 + 32 + i;
  }
  zones[13].tracks[5] = 150 + 32;
  zones[13].len = 6;
  zones[13].color = purple;
  zones[14].tracks[0] = 100 + 38;
  zones[14].tracks[1] = 100 + 39;
  zones[14].tracks[2] = 100 + 40;
  zones[14].tracks[3] = 150 + 41;
  zones[14].len = 4;
  zones[14].color = cyan;
  zones[15].tracks[0] = 150 + 45;
  zones[15].tracks[1] = 200 + 46;
  zones[15].tracks[2] = 250 + 44;
  zones[15].tracks[3] = 250 + 47;
  zones[15].tracks[4] = 300 + 46;
  zones[15].tracks[5] = 300 + 47;
  zones[15].tracks[6] = 350 + 47;
  zones[15].tracks[7] = 400 + 47;
  zones[15].tracks[8] = 450 + 47;
  zones[15].tracks[9] = 500 + 47;
  zones[15].tracks[10] = 550 + 46;
  zones[15].tracks[11] = 550 + 47;
  zones[15].tracks[12] = 600 + 47;
  zones[15].len = 13;
  zones[15].color = purple;
  for (int i = 0; i < 7; i++) {
    zones[16].tracks[i] = 36 + 800 + i;
  }
  zones[16].tracks[7] = 44 + 750;
  zones[16].tracks[8] = 45 + 700;
  zones[16].len = 9;
  zones[16].color = green;
  for (int i = 0; i < 5; i++) {
    zones[17].tracks[i] = 36 + 750 + i;
  }
  zones[17].tracks[5] = 700 + 41;
  zones[17].tracks[6] = 650 + 43;
  zones[17].len = 7;
  zones[17].color = green;
  for (int i = 0; i < 5; i++) {
    zones[18].tracks[i] = 750 + 30 + i;
  }
  zones[18].tracks[5] = 700 + 32;
  zones[18].len = 6;
  zones[18].color = yellow;
  zones[19].tracks[0] = 600 + 30;
  zones[19].len = 1;
  zones[19].color = cyan;
  zones[20].tracks[0] = 600 + 24;
  zones[20].len = 1;
  zones[20].color = cyan;
  zones[21].tracks[0] = 550 + 6;
  zones[21].tracks[1] = 550 + 7;
  zones[21].tracks[2] = 600 + 7;
  zones[21].tracks[3] = 600 + 9;
  zones[21].tracks[4] = 650 + 8;
  zones[21].tracks[5] = 650 + 10;
  zones[21].tracks[6] = 700 + 9;
  zones[21].tracks[7] = 700 + 11;
  zones[21].tracks[8] = 750 + 10;
  zones[21].tracks[9] = 750 + 12;
  zones[21].tracks[10] = 750 + 13;
  zones[21].tracks[11] = 750 + 14;
  zones[21].tracks[12] = 800 + 11;
  zones[21].tracks[13] = 800 + 12;
  zones[21].tracks[14] = 800 + 13;
  zones[21].tracks[15] = 800 + 14;
  zones[21].len = 16;
  zones[21].color = blue;
  for (int i = 0; i < 9; i++) {
    zones[22].tracks[i] = 750 + 16 + i;
  }
  zones[22].tracks[9] = 700 + 22;
  zones[22].len = 10;
  zones[22].color = purple;
  for (int i = 0; i < 3; i++) {
    zones[23].tracks[i] = 750 + i + 26;
  }
  zones[23].len = 3;
  zones[23].color = red;
  for (int i = 0; i < 10; i++) {
    zones[24].tracks[i] = 950 + 22 + i;
  }
  for (int i = 0; i < 9; i++) {
    zones[24].tracks[i + 10] = 800 + 16 + i;
  }
  for (int i = 0; i < 5; i++) {
    zones[24].tracks[10 + 9 + i] = 800 + i + 30;
  }
  zones[24].tracks[24] = 850 + 22;
  zones[24].tracks[25] = 850 + 32;
  zones[24].tracks[26] = 900 + 23;
  zones[24].tracks[27] = 900 + 31;
  zones[24].len = 28;
  zones[24].color = cyan;
  for (int i = 0; i < 3; i++) {
    zones[25].tracks[i] = 800 + i + 26;
  }
  zones[25].len = 3;
  zones[25].color = red;
  for (int i = 0; i < 13; i++) {
    zones[26].tracks[i] = 950 + 8 + i;
  }
  zones[26].tracks[13] = 900 + 7;
  zones[26].tracks[14] = 850 + 6;
  zones[26].tracks[15] = 800 + 5;
  zones[26].tracks[16] = 750 + 4;
  zones[26].tracks[17] = 700 + 3;
  zones[26].tracks[18] = 650 + 2;
  zones[26].len = 19;
  zones[26].color = yellow;
  zones[27].tracks[0] = 750 + 2;
  zones[27].len = 1;
  zones[27].color = green;
  for (int i = 0; i < 3; i++) {
    zones[28].tracks[i] = 850 + 2 + i;
  }
  zones[28].len = 3;
  zones[28].color = yellow;
  for (int i = 0; i < 5; i++) {
    zones[29].tracks[i] = 950 + 2 + i;
  }
  zones[29].len = 5;
  zones[29].color = green;
  for (int i = 0; i < 14; i++) {
    zones[30].tracks[i] = 950 + 33 + i;
  }
  zones[30].len = 14;
  zones[30].color = red;
  zones[31].tracks[0] = 750;
  zones[31].len = 1;
  zones[31].color = purple;
  zones[32].tracks[0] = 950;
  zones[32].len = 1;
  zones[32].color = blue;
  zones[33].tracks[0] = 850;
  zones[33].len = 1;
  zones[33].color = red;
  for (int i = 0; i < ZONE_NUMBERS; i++) {
    for (int j = 0; j < zones[i].len; j++) {
      zones[i].tracks[j] *= 2;
    }
  }
}

void zones_b_init() {
  // zone 0 is not used.
  zones_a_init();
  for (int i = 0; i < ZONE_NUMBERS; i++) {
    for (int j = 0; j < zones[i].len; j++) {
      int row = zones[i].tracks[j] / 100;
      int col = zones[i].tracks[j] % 100;
      row = 19 - row;
      col = 97 - col;
      zones[i].tracks[j] = row * 100 + col - 1;
    }
  }
  // modify zone[3]
  for (int i = 0; i < 8; i++) {
    zones[3].tracks[i] = (350 + 50 * i + 47) * 2 - 1;
  }
  zones[3].len = 8;
  zones[4].len = zones[4].len - 1;
  zones[26].len = zones[26].len - 1;
}

bool ReserveTrack(int zone_num, int train_index) {
  int terminal = WhoIs("terminal");

  if (zones[zone_num].reserved && zones[zone_num].reservedby != train_index) {
    TerminalLogPrint(
        terminal,
        "failed to reserve zone %d for %d, occupied by %d",
        zone_num,
        train_index,
        zones[zone_num].reservedby
    );
    return false;
  }

  zones[zone_num].reserved = true;
  zones[zone_num].reservedby = train_index;
  TerminalUpdateZoneReservation(terminal, zone_num, train_index, 0);
  TerminalLogPrint(terminal, "successfuly reserved zone %d for %d", zone_num, train_index);
  return true;
}

int ZoneOccupied(int zone) {
  return zones[zone].reservedby;
}

void ReleaseReservations(int zone) {
  int terminal = WhoIs("terminal");
  zones[zone].reserved = false;
  zones[zone].reservedby = -1;
  TerminalUpdateZoneReservation(terminal, zone, zones[zone].reservedby, 1);
}

struct Zone getZone(int zone) {
  return zones[zone];
}

// int main(){
//     int base_row = 2;
//     int base_col = 2;
//     zones_b_init();
//     printf("\033[2J");
//     printf(black);
//     printf(track_b);
//     for(int i=0;i<ZONE_NUMBERS;i++){
//         for(int j=0;j<zones[i].len;j++){
//             int row = zones[i].tracks[j]/100;
//             int col = zones[i].tracks[j]%100;
//             printf("\033[0;31m");
//             printf("\033[%d;%dH",row,col);
//             printf("**");
//         }
//     }
//     printf("\033[25;50H");
// }