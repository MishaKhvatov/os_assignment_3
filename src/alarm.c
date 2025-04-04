#include "alarm.h"
#include "console.h"
#include "errors.h"

/**
 * @brief Insert an alarm into a list in timestamp order
 *
 * @param list_head Pointer to the list head pointer
 * @param alarm Pointer to alarm structure to insert
 */
void insert_alarm_in_list(alarm_t **list_head, alarm_t *alarm) {
    alarm_t *current, *prev = NULL;

    current = *list_head;

    /* Find insertion point (ordered by timestamp) */
    while (current != NULL && current->time_stamp <= alarm->time_stamp) {
        prev = current;
        current = current->link;
    }

    /* Insert at head of list */
    if (prev == NULL) {
        alarm->link = *list_head;
        if (*list_head != NULL) {
            (*list_head)->prev = alarm;
        }
        *list_head = alarm;
    } else {
        /* Insert in middle or at end */
        alarm->link = current;
        alarm->prev = prev;
        prev->link = alarm;
        if (current != NULL) {
            current->prev = alarm;
        }
    }
}

/**
 * @brief Find an alarm in the alarm list by ID
 * 
 * @param list Pointer to the alarm list
 * @param alarm_id ID of the alarm to find
 * @return alarm_t* Pointer to the alarm or NULL if not found
 */
alarm_t *find_alarm_by_id(alarm_t *list, int alarm_id) {
    alarm_t *current = list;
    
    while (current != NULL) {
        if (current->alarm_id == alarm_id) {
            return current;
        }
        current = current->link;
    }
    
    return NULL;
}

/**
 * @brief Helper function to get a sorted list of all active group IDs from the alarm list
 * Uses more efficient algorithm by collecting all IDs in one pass, sorting, then removing duplicates
 *
 * @param list Pointer to the alarm list
 * @param group_ids Array to store group IDs
 * @param max_groups Maximum number of groups to store
 * @return int Number of unique group IDs found
 */
int get_active_group_ids(alarm_t *list, int *group_ids, int max_groups) {
    alarm_t *current;
    int count = 0;
    int unique_count = 0;
    
    /* First pass: collect all active group IDs */
    current = list;
    while (current != NULL && count < max_groups) {
        /* Only consider start alarms that are active or suspended */
        if ((current->type == REQ_START_ALARM || current->type == REQ_CHANGE_ALARM) &&
            (current->status == ALARM_ACTIVE || current->status == ALARM_SUSPENDED)) {
            
            group_ids[count++] = current->group_id;
        }
        current = current->link;
    }
    
    if (count == 0) {
        return 0;
    }
    
    /* Sort the collected group IDs */
    qsort(group_ids, count, sizeof(int), compare_ints);
    
    /* Second pass: remove duplicates */
    unique_count = 1;  /* First ID is always unique */
    for (int i = 1; i < count; i++) {
        if (group_ids[i] != group_ids[i-1]) {
            group_ids[unique_count++] = group_ids[i];
        }
    }
    
    return unique_count;
}

/**
 * @brief Comparison function for qsort
 */
int compare_ints(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

/**
 * @brief Checks if a group ID is the largest among active alarms
 *
 * @param list Pointer to the alarm list
 * @param group_id The group ID to check
 * @return int 1 if it's the largest, 0 otherwise
 */
int is_largest_group_id(alarm_t *list, int group_id) {
    int group_ids[100]; /* Reasonably large array for group IDs */
    int count;

    /* Get all unique active group IDs, sorted in ascending order */
    count = get_active_group_ids(list, group_ids, 100);
    if (count == 0) return 1; /* No groups, so yes by default */

    /* The largest is the last one in the sorted array */
    return (group_id == group_ids[count - 1]);
}

/**
 * @brief Parse user input and fill the alarm structure
 *
 * @param input User input string to parse
 * @param alarm Pointer to alarm structure to fill
 * @return int 0 on success, non-zero on error
 */
int parse_command(char *input, alarm_t *alarm) {
    int alarm_id, group_id, interval, time_value;
    char *message_start;
    
    /* Initialize the alarm structure */
    memset(alarm, 0, sizeof(alarm_t));
    alarm->status = ALARM_ACTIVE;
    alarm->time_stamp = time(NULL);

    /* Parse Start_Alarm command */
    if (sscanf(input, "Start_Alarm(%d): Group(%d) %d %d", 
              &alarm_id, &group_id, &interval, &time_value) == 4) {
        if (alarm_id <= 0 || group_id <= 0 || interval <= 0 || time_value <= 0) {
            return 1; /* Invalid parameters */
        }

        /* Find the start of the message */
        message_start = input;
        
        /* Skip past the time value */
        int fields_to_skip = 4; /* alarm_id, group_id, interval, time_value */
        for (int i = 0; i < fields_to_skip && message_start != NULL; i++) {
            message_start = strchr(message_start, ' ');
            if (message_start) {
                message_start++; /* Move past the space */
            }
        }
        
        /* Set message if found */
        if (message_start != NULL) {
            strncpy(alarm->message, message_start, MAX_MESSAGE_LEN - 1);
            alarm->message[MAX_MESSAGE_LEN - 1] = '\0'; /* Ensure null termination */
        }

        alarm->type = REQ_START_ALARM;
        alarm->alarm_id = alarm_id;
        alarm->group_id = group_id;
        alarm->interval = interval;
        alarm->time = time_value;
        alarm->expiry = alarm->time_stamp + time_value;
        return 0;
    }

    /* Parse Change_Alarm command */
    if (sscanf(input, "Change_Alarm(%d): Group(%d) %d", 
              &alarm_id, &group_id, &time_value) == 3) {
        if (alarm_id <= 0 || group_id <= 0 || time_value <= 0) {
            return 1; /* Invalid parameters */
        }

        /* Find the start of the message */
        message_start = input;
        
        /* Skip past the time value */
        int fields_to_skip = 3; /* alarm_id, group_id, time_value */
        for (int i = 0; i < fields_to_skip && message_start != NULL; i++) {
            message_start = strchr(message_start, ' ');
            if (message_start) {
                message_start++; /* Move past the space */
            }
        }
        
        /* Set message if found */
        if (message_start != NULL) {
            strncpy(alarm->message, message_start, MAX_MESSAGE_LEN - 1);
            alarm->message[MAX_MESSAGE_LEN - 1] = '\0'; /* Ensure null termination */
        }

        alarm->type = REQ_CHANGE_ALARM;
        alarm->alarm_id = alarm_id;
        alarm->group_id = group_id;
        alarm->interval = 0; /* Will be set by change alarm thread */
        alarm->time = time_value;
        alarm->expiry = alarm->time_stamp + time_value;
        return 0;
    }

    /* Parse Cancel_Alarm command */
    if (sscanf(input, "Cancel_Alarm(%d)", &alarm_id) == 1) {
        if (alarm_id <= 0) {
            return 1; /* Invalid parameters */
        }

        alarm->type = REQ_CANCEL_ALARM;
        alarm->alarm_id = alarm_id;
        return 0;
    }

    /* Parse Suspend_Alarm command */
    if (sscanf(input, "Suspend_Alarm(%d)", &alarm_id) == 1) {
        if (alarm_id <= 0) {
            return 1; /* Invalid parameters */
        }

        alarm->type = REQ_SUSPEND_ALARM;
        alarm->alarm_id = alarm_id;
        return 0;
    }

    /* Parse Reactivate_Alarm command */
    if (sscanf(input, "Reactivate_Alarm(%d)", &alarm_id) == 1) {
        if (alarm_id <= 0) {
            return 1; /* Invalid parameters */
        }

        alarm->type = REQ_REACTIVATE_ALARM;
        alarm->alarm_id = alarm_id;
        return 0;
    }

    /* Parse View_Alarms command */
    if (strcmp(input, "View_Alarms") == 0) {
        alarm->type = REQ_VIEW_ALARMS;
        return 0;
    }

    return 2; /* Unrecognized command format */
}
