/* This file defines data structures and declarations shared by all threads */
#ifndef ALARM_H
#define ALARM_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "errors.h"

/** Maximum length of alarm messages */
#define MAX_MESSAGE_LEN 128

/** Maximum length of alarm IDs */
#define MAX_ID_LEN 63

/** Maximum number of alarms per display thread */
#define MAX_ALARMS_PER_THREAD 2

/** Circular buffer size */
#define CIRCULAR_BUFFER_SIZE 4

typedef enum {
    ALARM_ACTIVE = 0,      /**< Alarm is active and will be displayed */
    ALARM_SUSPENDED = 1 << 0,   /**< Alarm has been suspended */
    ALARM_MOVED = 1 << 1,       /**< Alarm has been moved to a new display thread> */
    ALARM_REMOVE = 1 << 2      /**< Alarm is ready to be removed */
} alarm_status_t;

typedef enum {
    REQ_START_ALARM,    /**< Request to start a new alarm */
    REQ_CHANGE_ALARM,   /**< Request to change an existing alarm */
    REQ_CANCEL_ALARM,   /**< Request to cancel an existing alarm */
    REQ_SUSPEND_ALARM,  /**< Request to temporarily suspend an alarm */
    REQ_REACTIVATE_ALARM, /**< Request to reactivate a suspended alarm */
    REQ_VIEW_ALARMS     /**< Request to view all active alarms */
} alarm_req_type_t;

typedef struct alarm_tag {
    struct alarm_tag    *link;          /**< Link to next alarm in list */
    struct alarm_tag    *prev;          /**< Link to previous alarm in list */
    alarm_req_type_t    type;           /**< Type of alarm request */
    alarm_status_t      status;         /**< Status of alarm */
    time_t              time_stamp;     /**< Time when request was received */
    time_t              time;           /**< Seconds from now until expiration */
    time_t              expiry;         /**< Absolute time of expiry */
    int                 alarm_id;       /**< ID of this alarm */
    int                 group_id;       /**< Group ID of this alarm */
    int                 interval;       /**< Interval for periodic printing */
    char                message[MAX_MESSAGE_LEN]; /**< Message to display */
} alarm_t;

/* Function prototypes */
void insert_alarm_in_list(alarm_t **list_head, alarm_t *alarm);
alarm_t *find_alarm_by_id(alarm_t *list, int alarm_id);
int get_active_group_ids(alarm_t *list, int *group_ids, int max_groups);
int is_largest_group_id(alarm_t *list, int group_id);
int parse_command(char *input, alarm_t *alarm);
int compare_ints(const void *a, const void *b);

#endif /* ALARM_H */
