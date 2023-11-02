#include "distance.h"
#include "track_data.h"
#include "../../server/name_server.h"
#include "../trainset.h"
#include <stdbool.h>

struct TrackDistance track_distance(struct TrackNode *track, struct TrackNode begin){
    
    // struct TrackNode *track;
    // tracka_init(track);
    // from sensor A3
    // struct TrackNode begin = track[2];
    // struct TrackNode sensor = begin;
    struct TrackDistance ret = {.distance = -1, .begin = begin.name, .end = NULL};
    struct TrackNode node = begin;
    int train = WhoIs("train");
    int dist = 0;

    while(true){
        // nextnode = *sensor.edge[DIR_AHEAD].dest;
        // dist+=sensor.edge[DIR_AHEAD].dist;
        if ((dist!=0)&&(&node==&begin)){
            // loop
            ret.distance = dist;
            ret.end = node.name;
            return ret;
        }
        
        if (node.type== NODE_BRANCH){
            enum SwitchDirection direction= TrainGetSwitchState(train,node.num);
            if (direction==DIRECTION_CURVED){
                node = *node.edge[DIR_CURVED].dest;
                dist += node.edge[DIR_CURVED].dist;
            }else if (direction==DIRECTION_STRAIGHT){
                node = *node.edge[DIR_STRAIGHT].dest;
                dist += node.edge[DIR_STRAIGHT].dist;
            }else {
                return ret;
            }
        }else if (node.type == NODE_MERGE || node.type == NODE_SENSOR){
            node = *node.edge[DIR_AHEAD].dest;
            dist += node.edge[DIR_AHEAD].dist;
        }else if (node.type == NODE_EXIT){
            // reach the exit of the track
            ret.distance = dist;
            ret.end = node.name;
            return ret;
        }else {
            return ret;
        }
    }
    

}