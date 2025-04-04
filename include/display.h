#ifndef DISPLAY_H
#define DISPLAY_H

#include "alarm.h"
typedef struct alarm_snapshot_tag {
    alarm_status_t      status;         /**< Status of alarm */
    time_t              time_stamp;     /**< Time when request was received */
    time_t              time;           /**< Seconds from now until expiration */
    time_t              last_print_time; /*< Last print time*/
    int                 alarm_id;       /**< ID of this alarm */
    int                 group_id;       /**< Group ID of this alarm */
    int                 interval;       /**< Interval for periodic printing */
    char                message[MAX_MESSAGE_LEN]; /**< Message to display */
} alarm_snapshot_t;

typedef struct display_thread_tag {
    struct display_thread_tag *next;    /**< Link to next display thread */
    pthread_t thread_id;                /**< ID of this thread */
    pthread_mutex_t mutex;              /**< Mutex for this display thread */
    int group_id;                       /**< Group ID handled by this thread */
    alarm_t *alarm_1;                   /**< First alarm assigned to this thread */
    alarm_t *alarm_2;                   /**< Second alarm assigned to this thread */
    int alarm_count;                    /**< Count of alarms assigned (0, 1, or 2) */
} display_thread_t;

void create_snapshot(alarm_snapshot_t* snap, alarm_t* alarm);
void update_snapshot(alarm_snapshot_t* snapshot, alarm_t* alarm, pthread_t thread_id);
void periodic_print(alarm_snapshot_t* snapshot, pthread_t thread_id);
#endif /* DISPLAY_H */

