
/*This file contains important utilities for the display thread functionality*/
#include "display.h"
#include "console.h"

void create_snapshot(alarm_snapshot_t *snapshot, alarm_t *alarm) {
    if (snapshot == NULL || alarm == NULL) return;
    
    snapshot->alarm_id = alarm->alarm_id;
    snapshot->time_stamp = alarm->time_stamp;
    snapshot->status = (alarm->status); /*Alarm Moved is a live-only flag*/
    
    if(alarm->status & ALARM_MOVED) snapshot->status = (alarm->status) & (~ALARM_MOVED);
    
    snapshot->group_id = alarm->group_id;
    snapshot->interval = alarm->interval;
    snapshot->time = alarm->time;
    strncpy(snapshot->message, alarm->message, MAX_MESSAGE_LEN);
}

/**
* This function updates the snapshot to correspond with the alarm, and prints the corresponding print message.
* NOTES: 
* 1. We assume the console has been initilized for printing
* 2. We assume that we have the reader lock
* 3. The change alarm thread must set the ALARM_MOVED flag if the group has been changed, to signal to the display thread that it needs to print the 
* message about taking over displaying the alarm. If the alarm is later removed, the display thread must reset the snapshot to not have the ALARM_MOVED 
* flag.
*/
void update_snapshot(alarm_snapshot_t* snapshot, alarm_t* alarm, pthread_t thread_id){
    time_t current_time = time((void*)0);
    
    /*We assume that NULL means the alarm has been removed by the cancel alarm thread*/
    if(alarm == (void*)0){
        console_print("Display Thread %lx Has Stopped Printing Message of Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
            (long)thread_id,
            snapshot->alarm_id,
            (long)current_time,
            snapshot->group_id,
            (long)snapshot->time_stamp,
            snapshot->interval,
            (int)snapshot->time,
            snapshot->message);
        snapshot->status = ALARM_REMOVE;
        return;
    }
    
    /*A3.8.5*/
    if(alarm->expiry <= current_time){
        console_print("Display Thread %lx Has Stopped Printing Expired Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
             (long)thread_id,
             snapshot->alarm_id,
             (long)current_time,
             snapshot->group_id,
             (long)snapshot->time_stamp,
             snapshot->interval,
             (int)snapshot->time,
             snapshot->message);
         snapshot->status = ALARM_REMOVE;
         return;
    }
    
    /*A3.8.6*/
    
    /*We are the old display thread*/
    if(alarm->group_id != snapshot->group_id){
            console_print("Display Thread %lx Has Stopped Printing Message of Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
            (long)thread_id,
            alarm->alarm_id,
            (long)current_time,
            alarm->group_id,
            (long)alarm->time_stamp,
            alarm->interval,
            (int)alarm->time,
            alarm->message);
            snapshot->status = ALARM_REMOVE;
            return;
    }

    /*We are the new display thread*/
    /*ALARM_MOVED is a flag that communicates information about if the alarm was originally in the group it's in currently. 
     * If alarm has ALARM_MOVED but snapshot does not, the alarm just moved into the new group from a previous one. 
     * We update snapshot to have this flag to indicate we acknowledged the move. 
     * 
     * Alarm        | Snapshot     | Result
     * -----------------------------------------------------------
     *  ALARM MOVED | ALARM_MOVED  | We already processed the move
     *  ALARM MOVED | !ALARM_MOVED | alarm just moved in
     * */
    if( (alarm->status & ALARM_MOVED ) && !(snapshot->status & ALARM_MOVED)){
        console_print("Display Thread %lx Has Taken Over Printing Message of Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
            (long)thread_id,
            alarm->alarm_id,
            (long)current_time,
            alarm->group_id,
            (long)alarm->time_stamp,
            alarm->interval,
            (int)alarm->time,
            alarm->message);
            snapshot -> status = ALARM_MOVED;
            return;
    }

    /*A3.8.7*/
    if (strcmp(alarm->message, snapshot->message) != 0) {
        console_print("Display Thread %lx Starts to Print Changed Message Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
                     (long)thread_id,
                     alarm->alarm_id,
                     (long)current_time,
                     alarm->group_id,
                     (long)alarm->time_stamp,
                     alarm->interval,
                     (int)alarm->time,
                     alarm->message);

        // Copy new message into snapshot
        strncpy(snapshot->message, alarm->message, sizeof(snapshot->message) - 1);
        snapshot->message[sizeof(snapshot->message) - 1] = '\0';  // Ensure null termination
    }
    if(alarm->interval != snapshot->interval){
        console_print("Display Thread %lx Starts to Print Changed Interval Value Alarm(%d) at %ld: Group(%d) %ld %d %d %s",
                (long)thread_id,
                alarm->alarm_id,
                (long)current_time,
                alarm->group_id,
                (long) alarm->time_stamp,
                alarm->interval,
                (int) alarm->time,
                alarm->message);
        snapshot->interval = alarm->interval;
    }
    
    snapshot->status = alarm->status;
}

/*
 * Funciton to periodically print the alarm, assumes that the snapshot is not null. All null-handling should be done by the main thread function.
 * */
void periodic_print(alarm_snapshot_t* snapshot, pthread_t thread_id){
    time_t current_time = time((void*)0);
    if(snapshot->status == ALARM_REMOVE || snapshot->status == ALARM_SUSPENDED) return; 
    
    if(current_time - snapshot->last_print_time > snapshot->interval){
        console_print("Alarm (%d) Printed by Alarm Display Thread %lx at %ld: Group(%d) %ld %d %d %s",
                      snapshot->alarm_id,
                      (long)thread_id,
                      (long)current_time,
                      snapshot->group_id,
                      (long)snapshot->time_stamp,
                      snapshot->interval,
                      (int)snapshot->time,
                      snapshot->message);
        snapshot->last_print_time = current_time;
    }
}

