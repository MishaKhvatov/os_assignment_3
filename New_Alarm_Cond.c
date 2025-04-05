#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <semaphore.h>
#include "alarm.h"
#include "display.h"
#include "console.h"
#include "errors.h"
#include "circular_buffer.h"


/* Global variables */
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t change_alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t change_alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t manage_alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t view_alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t remove_alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t suspend_reactivate_cond = PTHREAD_COND_INITIALIZER;

/* Reader-Writer semaphores for alarm list */
sem_t read_count_mutex;
sem_t alarm_list_mutex;
sem_t write_mutex;
int read_count = 0;

/* Shared data structures */
alarm_t *alarm_list = NULL;
display_thread_t *display_threads = NULL;
alarm_t *change_alarm_list = NULL;
int most_recent_displayed_alarm_id = -1;

circular_buffer_t alarm_buffer;

/* Helper functions for alarm list operations */
alarm_t *find_most_recent_alarm_by_type(alarm_t *list, alarm_req_type_t type);
alarm_t *find_alarm_by_type(alarm_t *list, alarm_req_type_t type);

void reader_lock() {
    sem_wait(&read_count_mutex);
    read_count++;
    if (read_count == 1) {
        sem_wait(&write_mutex);
    }
    sem_post(&read_count_mutex);
    sem_wait(&alarm_list_mutex);
}

/* Function to implement reader unlock */
void reader_unlock() {
    sem_post(&alarm_list_mutex);
    sem_wait(&read_count_mutex);
    read_count--;
    if (read_count == 0) {
        sem_post(&write_mutex);
    }
    sem_post(&read_count_mutex);
}

/* Function to implement writer lock */
void writer_lock() {
    sem_wait(&write_mutex);
}

/* Function to implement writer unlock */
void writer_unlock() {
    sem_post(&write_mutex);
}


/**
 * @brief Determines if a group ID is the next in sequence to display
 *
 * @param group_id The group ID to check
 * @return int 1 if it's the next group to display, 0 otherwise
 */
int is_next_group_to_display(int group_id) {
    int group_ids[100]; /* Reasonably large array for group IDs */
    int count, i, last_displayed_idx = -1;
    alarm_t *current;
    int last_group_id = -1;

    /* Get all unique active group IDs, sorted in ascending order */
    count = get_active_group_ids(alarm_list, group_ids, 100);
    if (count == 0) return 1; /* No other groups, so yes */
    if (count == 1) return (group_ids[0] == group_id); /* Only one group */

    /* Find the group ID of the most recently displayed alarm */
    if (most_recent_displayed_alarm_id >= 0) {
        current = alarm_list;
        while (current != NULL) {
            if (current->alarm_id == most_recent_displayed_alarm_id) {
                last_group_id = current->group_id;
                break;
            }
            current = current->link;
        }

        /* Find its index in our sorted array */
        for (i = 0; i < count; i++) {
            if (group_ids[i] == last_group_id) {
                last_displayed_idx = i;
                break;
            }
        }
    }

    /* If we couldn't find the last displayed group, start with the smallest */
    if (last_displayed_idx == -1) {
        return (group_id == group_ids[0]);
    }

    /* The next group is the one after the last displayed */
    int next_idx = (last_displayed_idx + 1) % count;
    return (group_id == group_ids[next_idx]);
}
/**
 * @brief Helper function to return alarm type in a string format
 *
 * @param in type of alarm request
 * @return alarm type in a string
 */
const char* alarm_type_to_string(int type) {
    switch (type) {
        case REQ_START_ALARM:
            return "Start_Alarm";
        case REQ_CHANGE_ALARM:
            return "Change_Alarm";
        case REQ_CANCEL_ALARM:
            return "Cancel_Alarm";
        case REQ_SUSPEND_ALARM:
            return "Suspend_Alarm";
        case REQ_REACTIVATE_ALARM:
            return "Reactivate_Alarm";
        case REQ_VIEW_ALARMS:
            return "View_Alarms";
        default:
            return "Unknown";
    }
}


/**
 * @brief Consumer thread function
 *
 * Retrieves alarm requests from the circular buffer and processes them
 *
 * @param arg Thread argument (not used)
 * @return void* Always NULL
 */
void *consumer_thread(void *arg) {
    alarm_t *alarm;
    int index;
    time_t retrieve_time;
    
    while (1) {
        /* Retrieve alarm from circular buffer */
        alarm = circular_buffer_remove(&alarm_buffer, &index);
        retrieve_time = time(NULL);
        
        /* Print retrieval message */
        const char *type_str;
        switch (alarm->type) {
            case REQ_START_ALARM: type_str = "Start_Alarm"; break;
            case REQ_CHANGE_ALARM: type_str = "Change_Alarm"; break;
            case REQ_CANCEL_ALARM: type_str = "Cancel_Alarm"; break;
            case REQ_SUSPEND_ALARM: type_str = "Suspend_Alarm"; break;
            case REQ_REACTIVATE_ALARM: type_str = "Reactivate_Alarm"; break;
            case REQ_VIEW_ALARMS: type_str = "View_Alarms"; break;
            default: type_str = "Unknown"; break;
        }
        
        console_print("Consumer Thread has Retrieved %s Request(%d) at %ld: %ld from Circular_Buffer Index: %d",
                     type_str, alarm->alarm_id, (long)retrieve_time, (long)alarm->time_stamp, index);
        
        /* Process alarm based on type */
        switch (alarm->type) {
            case REQ_START_ALARM:
                writer_lock();
                insert_alarm_in_list(&alarm_list, alarm);
                writer_unlock();
                
                console_print("Start_Alarm(%d) Inserted by Consumer Thread %ld Into Alarm List: Group(%d) %ld %d %ld %s",
                             alarm->alarm_id, (long)pthread_self(), alarm->group_id, 
                             (long)alarm->time_stamp, alarm->interval, (long)alarm->time, alarm->message);
                
                /* Signal the start alarm thread */
                pthread_cond_signal(&start_alarm_cond);
                break;
                
            case REQ_CHANGE_ALARM:
                writer_lock();
                insert_alarm_in_list(&change_alarm_list, alarm);
                writer_unlock();
                
                console_print("Change_Alarm(%d) Inserted by Consumer Thread %ld into Separate Change Alarm Request List: Group(%d) %ld %d %ld %s",
                             alarm->alarm_id, (long)pthread_self(), alarm->group_id, 
                             (long)alarm->time_stamp, alarm->interval, (long)alarm->time, alarm->message);
                
                /* Signal the change alarm thread */
                pthread_cond_signal(&change_alarm_cond);
                break;
                
            case REQ_CANCEL_ALARM:
                writer_lock();
                insert_alarm_in_list(&alarm_list, alarm);
                writer_unlock();
                
                console_print("Cancel_Alarm(%d) Inserted by Consumer Thread %ld Into Alarm List: %ld",
                             alarm->alarm_id, (long)pthread_self(), (long)alarm->time_stamp);
                
                /* Signal the remove alarm thread */
                pthread_cond_signal(&remove_alarm_cond);
                break;
                
            case REQ_SUSPEND_ALARM:
                writer_lock();
                insert_alarm_in_list(&alarm_list, alarm);
                writer_unlock();
                
                console_print("Suspend_Alarm(%d) Inserted by Consumer Thread %ld Into Alarm List: %ld",
                             alarm->alarm_id, (long)pthread_self(), (long)alarm->time_stamp);
                
                /* Signal the suspend/reactivate alarm thread */
                pthread_cond_signal(&suspend_reactivate_cond);
                break;
                
            case REQ_REACTIVATE_ALARM:
                writer_lock();
                insert_alarm_in_list(&alarm_list, alarm);
                writer_unlock();
                
                console_print("Reactivate_Alarm(%d) Inserted by Consumer Thread %ld Into Alarm List: %ld",
                             alarm->alarm_id, (long)pthread_self(), (long)alarm->time_stamp);
                
                /* Signal the suspend/reactivate alarm thread */
                pthread_cond_signal(&suspend_reactivate_cond);
                break;
                
            case REQ_VIEW_ALARMS:
                writer_lock();
                insert_alarm_in_list(&alarm_list, alarm);
                writer_unlock();
                
                console_print("View_Alarms Request Inserted by Consumer Thread %ld Into Alarm List: %ld",
                             (long)pthread_self(), (long)alarm->time_stamp);
                
                /* Signal the view alarms thread */
                pthread_cond_signal(&view_alarm_cond);
                break;
        }
    }
    
    return NULL;
}


void* display_alarm_thread (void* arg){
    display_thread_t *dt = (display_thread_t*) arg;

    if (dt == NULL) {
        return NULL;
    }

    alarm_snapshot_t *snapshot_1 = NULL;
    alarm_snapshot_t *snapshot_2 = NULL;
    time_t last_print_time_1 = 0;
    time_t last_print_time_2 = 0;
    int can_display = 0;
    time_t current_time;
    int status;

    /* Initialize the snapshots if alarms are already present */
    pthread_mutex_lock(&dt->mutex);
    if (dt->alarm_1 != NULL) {
        snapshot_1 = malloc(sizeof(alarm_snapshot_t));
        if (snapshot_1 == NULL) {
            pthread_mutex_unlock(&dt->mutex);
            errno_abort("Failed to allocate memory for alarm snapshot");
            return NULL;
        }
        memset(snapshot_1, 0, sizeof(alarm_snapshot_t));
        reader_lock();
        create_snapshot(snapshot_1, dt->alarm_1);
        reader_unlock();
    }

    if (dt->alarm_2 != NULL) {
        snapshot_2 = malloc(sizeof(alarm_snapshot_t));
        if (snapshot_2 == NULL) {
            pthread_mutex_unlock(&dt->mutex);
            if (snapshot_1) free(snapshot_1);
            errno_abort("Failed to allocate memory for alarm snapshot");
            return NULL;
        }
        memset(snapshot_2, 0, sizeof(alarm_snapshot_t));
        reader_lock();
        create_snapshot(snapshot_2, dt->alarm_2);
        reader_unlock();
    }
    pthread_mutex_unlock(&dt->mutex);

    while(1){
        sleep(1);
        current_time = time((void*)0);

        /* Check if we have zero alarms and should exit */
        if (dt->alarm_count == 0) {
            console_print("No More Alarms in Group(%d): Display Thread %lx exiting at %ld",
                dt->group_id, (long)dt->thread_id, (long)current_time);

            if (snapshot_1) free(snapshot_1);
            if (snapshot_2) free(snapshot_2);

            return NULL;
        }

        /* Initialize snapshots for newly added alarms */
        if (dt->alarm_1 != NULL && snapshot_1 == NULL) {
            snapshot_1 = malloc(sizeof(alarm_snapshot_t));
            if (snapshot_1 != NULL) {
                memset(snapshot_1, 0, sizeof(alarm_snapshot_t));
                reader_lock();
                create_snapshot(snapshot_1, dt->alarm_1);
                reader_unlock();
            }
        }

        if (dt->alarm_2 != NULL && snapshot_2 == NULL) {
            snapshot_2 = malloc(sizeof(alarm_snapshot_t));
            if (snapshot_2 != NULL) {
                memset(snapshot_2, 0, sizeof(alarm_snapshot_t));
                reader_lock();
                create_snapshot(snapshot_2, dt->alarm_2);
                reader_unlock();
            }
        }

        reader_lock();


        /* Logic for checking round-robin synchronization */
        if (!is_next_group_to_display(dt->group_id)) {
            pthread_mutex_unlock(&display_mutex);
            reader_unlock();
            continue;
        }

        if(snapshot_1 != (void*)0){
            update_snapshot(snapshot_1, dt->alarm_1, dt->thread_id);
            periodic_print(snapshot_1, pthread_self());
            /* Mark this thread's alarms as most recently displayed */
            pthread_mutex_lock(&display_mutex);
            most_recent_displayed_alarm_id = snapshot_1->alarm_id;

            /* If this is the largest group ID, reset for next cycle */
            if (is_largest_group_id(alarm_list, dt->group_id)) {
                most_recent_displayed_alarm_id = -1;
            }
            pthread_mutex_unlock(&display_mutex);
    
            if (snapshot_1->status == ALARM_REMOVE) {
                free(snapshot_1);
                snapshot_1 = NULL;
                dt->alarm_count--;
            }
        }

        
        if(snapshot_2 != (void*)0){
            update_snapshot(snapshot_2, dt->alarm_2, dt->thread_id);
            periodic_print(snapshot_2, pthread_self());
            /* Mark this thread's alarms as most recently displayed */
            pthread_mutex_lock(&display_mutex);
            most_recent_displayed_alarm_id = snapshot_2->alarm_id;
            /* If this is the largest group ID, reset for next cycle */
            if (is_largest_group_id(alarm_list,dt->group_id)) {
                most_recent_displayed_alarm_id = -1;
            }
            pthread_mutex_unlock(&display_mutex);

            if (snapshot_2->status == ALARM_REMOVE) {
                free(snapshot_1);
                snapshot_1 = NULL;
                dt->alarm_count--;
            }
        }
        reader_unlock();
        pthread_mutex_unlock(&dt->mutex);

    }
}
/**
 * Creates a new display thread for the specified group ID and initial alarm
 *
 * @param group_id Group ID this thread will handle
 * @param alarm Initial alarm to be assigned to this thread
 * @return Pointer to the newly created display thread, or NULL on failure
 */
display_thread_t *create_display_thread(int group_id, alarm_t *alarm) {
    display_thread_t *new_dt;
    int status;

    if (alarm == NULL) {
        errno_abort("Cannot create a display thread without an alarm");
        return NULL;
    }

    new_dt = (display_thread_t *)malloc(sizeof(display_thread_t));
    if (new_dt == NULL) {
        errno_abort("Failed to allocate memory for display thread");
        return NULL;
    }

    /* Initialize the display thread */
    memset(new_dt, 0, sizeof(display_thread_t));
    new_dt->group_id = group_id;
    new_dt->alarm_1 = alarm;  // Assign the initial alarm
    new_dt->alarm_count = 1;  // Set initial alarm count
    
    status = pthread_mutex_init(&new_dt->mutex, NULL);
    if (status != 0) {
        free(new_dt);
        errno_abort("Failed to initialize display thread mutex");
        return NULL;
    }

    /* Create the thread */
    status = pthread_create(&new_dt->thread_id, NULL, display_alarm_thread, (void *)new_dt);
    if (status != 0) {
        pthread_mutex_destroy(&new_dt->mutex);
        free(new_dt);
        errno_abort("Failed to create display thread");
        return NULL;
    }

    /* Add to the head of the list */
    pthread_mutex_lock(&display_mutex);
    new_dt->next = display_threads;
    display_threads = new_dt;
    pthread_mutex_unlock(&display_mutex);

    console_print("New Display Alarm Thread %ld Created for Group(%d) at %ld",
        (long)new_dt->thread_id, new_dt->group_id, (long)time(NULL));

    return new_dt;
}

/* Function prototypes for other threads - implementation to be added later */
void *start_alarm_thread(void *arg);
void *change_alarm_thread(void *arg);
void *suspend_reactivate_alarm_thread(void *arg);
void *remove_alarm_thread(void *arg);
void *view_alarms_thread(void *arg);

int main(int argc, char *argv[]) {
    pthread_t consumer_tid;
    pthread_t start_alarm_tid;
    pthread_t change_alarm_tid;
    pthread_t suspend_reactivate_tid;
    pthread_t remove_alarm_tid;
    pthread_t view_alarms_tid;
    int status;
    char *input_line;
    alarm_t *alarm;

    /* Initialize the console */
    console_init();

    /* Initialize the circular buffer */
    circular_buffer_init(&alarm_buffer);

    /* Initialize reader-writer semaphores */
    sem_init(&read_count_mutex, 0, 1);
    sem_init(&alarm_list_mutex, 0, 1);
    sem_init(&write_mutex, 0, 1);

    /* Print welcome message */
    console_print("Alarm System Initialized. Enter commands in the following formats:");
    console_print("  Start_Alarm(ID): Group(Group_ID) Interval Time Message");
    console_print("  Change_Alarm(ID): Group(Group_ID) Time Message");
    console_print("  Cancel_Alarm(ID)");
    console_print("  Suspend_Alarm(ID)");
    console_print("  Reactivate_Alarm(ID)");
    console_print("  View_Alarms");
    console_print("  quit or exit to terminate the program");

    /* Create worker threads */
    status = pthread_create(&consumer_tid, NULL, consumer_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create consumer thread");
    }

    status = pthread_create(&start_alarm_tid, NULL, start_alarm_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create start alarm thread");
    }

    status = pthread_create(&change_alarm_tid, NULL, change_alarm_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create change alarm thread");
    }

    status = pthread_create(&suspend_reactivate_tid, NULL, suspend_reactivate_alarm_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create suspend reactivate alarm thread");
    }

    status = pthread_create(&remove_alarm_tid, NULL, remove_alarm_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create remove alarm thread");
    }

    status = pthread_create(&view_alarms_tid, NULL, view_alarms_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create view alarms thread");
    }

    /* Process user input */
    while (1) {
        /* Get user input */
        input_line = input();
        if (input_line == NULL) {
            continue; /* Handle error or empty input */
        }

        /* Skip empty lines */
        if (strlen(input_line) == 0) {
            free(input_line);
            continue;
        }

        /* Exit command */
        if (strcmp(input_line, "exit") == 0 || strcmp(input_line, "quit") == 0) {
            free(input_line);
            break;
        }

        /* Create and initialize alarm structure */
        alarm = (alarm_t *)malloc(sizeof(alarm_t));
        if (alarm == NULL) {
            console_print("Error: Failed to allocate memory for alarm");
            free(input_line);
            continue;
        }

        /* Parse the command */
        int parse_result = parse_command(input_line, alarm);
        if (parse_result != 0) {
            /* Display appropriate error message based on parse_result */
            switch (parse_result) {
                case 1:
                    console_print("Error: Invalid parameters (IDs, interval, or time must be positive)");
                    break;
                case 2:
                    console_print("Error: Unrecognized command format");
                    break;
                default:
                    console_print("Error: Unknown parsing error");
            }

            /* Clean up and continue */
            free(alarm);
            free(input_line);
            continue;
        }

        /* Insert the alarm into the circular buffer */
        int insert_index = circular_buffer_insert(&alarm_buffer, alarm);

        /* Print appropriate message based on alarm type */
        const char *type_str;
        switch (alarm->type) {
            case REQ_START_ALARM: type_str = "Start_Alarm"; break;
            case REQ_CHANGE_ALARM: type_str = "Change_Alarm"; break;
            case REQ_CANCEL_ALARM: type_str = "Cancel_Alarm"; break;
            case REQ_SUSPEND_ALARM: type_str = "Suspend_Alarm"; break;
            case REQ_REACTIVATE_ALARM: type_str = "Reactivate_Alarm"; break;
            case REQ_VIEW_ALARMS: type_str = "View_Alarms"; break;
            default: type_str = "Unknown"; break;
        }

        console_print("Alarm Thread has Inserted %s Request(%d) at %ld: %ld into Circular_Buffer Index: %d",
                     type_str, alarm->alarm_id, (long)time(NULL), (long)alarm->time_stamp, insert_index);

        /* Clean up */
        free(input_line);
    }

    /* Clean up and exit */
    console_print("Exiting alarm system...");

    /* Cleanup semaphores */
    sem_destroy(&read_count_mutex);
    sem_destroy(&alarm_list_mutex);
    sem_destroy(&write_mutex);

    return 0;
}

/* Placeholder functions for other threads - to be implemented later */

void *start_alarm_thread(void *arg) {
    alarm_t *alarm;
    display_thread_t *dt, *target_dt;
    int found, count;
    time_t current_time;
    
    while (1) {
        /* Wait for a start alarm request */
        pthread_mutex_lock(&alarm_mutex);
        while (find_alarm_by_type(alarm_list, REQ_START_ALARM) == NULL) {
            pthread_cond_wait(&start_alarm_cond, &alarm_mutex);
        }
        
        /* Find the most recent start alarm request */
        alarm = find_most_recent_alarm_by_type(alarm_list, REQ_START_ALARM);
        if (alarm == NULL) {
            pthread_mutex_unlock(&alarm_mutex);
            continue;
        }

        /* Critical fix - set status and keep alarm in main list */
        alarm->status = ALARM_ACTIVE;
        pthread_mutex_unlock(&alarm_mutex);
        
        /* Assign to display thread */
        found = 0;
        target_dt = NULL;
        count = 0;
        
        pthread_mutex_lock(&display_mutex);
        dt = display_threads;
        while (dt != NULL) {
            if (dt->group_id == alarm->group_id) {
                count++;
                pthread_mutex_lock(&dt->mutex);
                if (dt->alarm_count < 2) {
                    target_dt = dt;
                    found = 1;
                }
                pthread_mutex_unlock(&dt->mutex);
                if (found) break;
            }
            dt = dt->next;
        }
        
        current_time = time(NULL);
        
        if (!found || count == 0) {
            dt = create_display_thread(alarm->group_id, alarm);
            if (dt != NULL) {
                console_print("Start Alarm Thread Created New Display Alarm Thread %ld For Alarm(%d) at %ld: Group(%d) %ld %d %ld %s",
                             (long)dt->thread_id, alarm->alarm_id, (long)current_time,
                             alarm->group_id, (long)alarm->time_stamp, alarm->interval,
                             (long)alarm->time, alarm->message);
            }
        } else {
            pthread_mutex_lock(&target_dt->mutex);
            if (target_dt->alarm_1 == NULL) {
                target_dt->alarm_1 = alarm;
            } else {
                target_dt->alarm_2 = alarm;
            }
            target_dt->alarm_count++;
            pthread_mutex_unlock(&target_dt->mutex);
            
            console_print("Alarm(%d) Assigned to Display Thread(%ld) at %ld: Group(%d) %ld %d %ld %s",
                          alarm->alarm_id, (long)target_dt->thread_id, (long)current_time,
                          alarm->group_id, (long)alarm->time_stamp, alarm->interval,
                          (long)alarm->time, alarm->message);
        }
        pthread_mutex_unlock(&display_mutex);
    }
    return NULL;
}

/* Helper function to find the most recent alarm of a given type */
alarm_t *find_most_recent_alarm_by_type(alarm_t *list, alarm_req_type_t type) {
    alarm_t *current = list;
    alarm_t *most_recent = NULL;
    
    while (current != NULL) {
        if (current->type == type) {
            if (most_recent == NULL || current->time_stamp > most_recent->time_stamp) {
                most_recent = current;
            }
        }
        current = current->link;
    }
    
    return most_recent;
}

/* Helper function to find any alarm of a given type */
alarm_t *find_alarm_by_type(alarm_t *list, alarm_req_type_t type) {
    alarm_t *current = list;
    
    while (current != NULL) {
        if (current->type == type) {
            return current;
        }
        current = current->link;
    }
    
    return NULL;
}

/**
 * @brief Change alarm thread function stub
 *
 * @param arg Thread argument (not used)
 * @return void* Always 0
 */
void *change_alarm_thread(void *arg) {
    alarm_t *change_alarm;
    int status;

    while (1) {
        /* Wait for the condition signal before proceeding */
        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0) {
            errno_abort("Failed to lock alarm mutex");
        }
        while (change_alarm_list == NULL) {
            status = pthread_cond_wait(&change_alarm_cond, &alarm_mutex);
            if (status != 0) {
                pthread_mutex_unlock(&alarm_mutex);
                errno_abort("Failed to wait on change_alarm_cond");
            }
        }

        status = pthread_mutex_lock(&change_alarm_mutex);
        if (status != 0) {
            errno_abort("Failed to lock alarm mutex");
        }
        writer_lock();
        /* The alarm(s) in change_alarm_list will be processed */
        change_alarm = change_alarm_list;
        while (change_alarm != NULL) {
            alarm_t *alarm = find_alarm_by_id(alarm_list, change_alarm->alarm_id);
            if (alarm_list == NULL) {
                console_print("NO ALARMS");
            }
            if (alarm == NULL) {
                /* Handle invalid change request */
                console_print("Invalid Change Alarm Request(%d at %ld: Group(%d) %ld %ld %s",
                change_alarm->alarm_id, (long)time(NULL), 
                change_alarm->group_id, change_alarm->time_stamp, 
                change_alarm->time, change_alarm->message);
            } else {
                /* Make required changes to alarm */
                alarm->time = change_alarm->time;
                alarm->expiry = change_alarm->expiry;
                strncpy(alarm->message, change_alarm->message, MAX_MESSAGE_LEN);
                if (alarm->group_id != change_alarm->group_id) {
                    alarm->status = ALARM_MOVED;
                
                    alarm->group_id = change_alarm->group_id;
                }

                console_print("Change Alarm Thread %ld Has Changed Alarm(%d at %ld: Group(%d) <%ld %d %ld %s>",
                (long)pthread_self(), alarm->alarm_id, (long)time(NULL), 
                alarm->group_id, alarm->time_stamp, alarm->interval, 
                alarm->time, alarm->message);
            }

            /* Delete change_alarm from the change_alarm_list */
            alarm_t *link = change_alarm->link;
            if (change_alarm->prev == NULL) {
                change_alarm_list = change_alarm->link;
            } else {
                change_alarm->prev->link = change_alarm->link;
            }

            change_alarm = link;
        }

        writer_unlock();

        status = pthread_mutex_unlock(&change_alarm_mutex);
        if (status != 0) {
            errno_abort("Failed to unlock alarm mutex");
        }
        
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0) {
            errno_abort("Failed to unlock alarm mutex");
        }

    }
    return NULL;
}

void *suspend_reactivate_alarm_thread(void *arg) {
    time_t curr_time;
    alarm_t *curr;
    alarm_t *next;
    alarm_t *req = NULL;
    alarm_t *mod_alarm = NULL;

    /** Lock in this process for safety **/
    pthread_mutex_lock(&alarm_mutex);
    while (1) {        
        /** Determine whether there are any requests for suspension or reactivation**/
        int received = 0;
        curr = alarm_list;
        while (curr) {
            if (curr->type == REQ_SUSPEND_ALARM || curr->type == REQ_REACTIVATE_ALARM) {
                received = 1;
                break;
            }
            curr = curr->link;
        }
        /** If we don't have anything to receive, we just wait **/
        if (!received) {
            pthread_cond_wait(&suspend_reactivate_cond, &alarm_mutex);
        } else {
            break;
        }
        /** Unlock here now **/
        pthread_mutex_unlock(&alarm_mutex);

        writer_lock();
        curr_time = time(NULL);

        /** Obtain a reactivation or suspension request htat is the most recent one**/
        req = NULL;
        curr = alarm_list;
        while (curr) {
            if ((curr->type == REQ_SUSPEND_ALARM || curr->type == REQ_REACTIVATE_ALARM) && (!req || curr->time_stamp > req->time_stamp)) {
                req = curr;
            }
            curr = curr->link;
        }

        /** If we have a request, then we can proceed with proessing it **/
        if (req) {
            /** We need the REQ_START_ALARM and the earlier timestamp**/
            mod_alarm = NULL;
            curr = alarm_list;
            while (curr) {
                /** Here, we check if the type is actually REQ_START_ALARM and the timestamp **/
                if (curr->type == REQ_START_ALARM && (curr->alarm_id == req->alarm_id) && (curr->time_stamp < req->time_stamp)) {
                    if (!mod_alarm || curr->time_stamp > mod_alarm->time_stamp) {
                        mod_alarm = curr;
                    }
                }
                curr = curr->link;
            }
            if (mod_alarm) {
                /** Situation 1: Suspension**/
                if (req->type == REQ_SUSPEND_ALARM && mod_alarm && mod_alarm->status == ALARM_ACTIVE) {
                    mod_alarm->status = ALARM_SUSPENDED;
                    console_print("Alarm(%d) Suspended at %ld: Group(%d) %ld %ld %s", mod_alarm->alarm_id, (long)curr_time, mod_alarm->group_id, (long)mod_alarm->time_stamp, (long)mod_alarm->time, mod_alarm->message);
                } 
                /** Situation 2: Reactivation**/
                else if (req->type == REQ_REACTIVATE_ALARM && mod_alarm && mod_alarm->status == ALARM_SUSPENDED) {
                    mod_alarm->status = ALARM_ACTIVE;
                    console_print("Alarm(%d) Reactivated at %ld: Group(%d) %ld %ld %s", mod_alarm->alarm_id, (long)curr_time, mod_alarm->group_id, (long)mod_alarm->time_stamp, (long)mod_alarm->time, mod_alarm->message);
                }
                
                /** Remove requests here **/
                if (req->prev) {
                    req->prev->link = req->link;
                } else {
                    alarm_list = req->link;
                }

                if (req->link)
                    req->link->prev = req->prev;
                free(req);
            }
        }   
        writer_unlock(); 
    }
    return NULL;
}

void *remove_alarm_thread(void *arg) {
    /* Implementation to be added */
    return NULL;
}

void* view_alarms_thread(void* arg) {
    time_t view_time = time(NULL);
    alarm_t *current_alarm;

    // Locking to safely access shared alarm list
    reader_lock();

    // Print the header for view alarms
    console_print("View Alarms at View Time %ld:", (long)view_time);

    // Wait for the consumer thread to signal the condition
    pthread_mutex_lock(&alarm_mutex);
    while (alarm_list == NULL) {
        pthread_cond_wait(&view_alarm_cond, &alarm_mutex);  // Wait for signal
    }

    current_alarm = alarm_list;
    while (current_alarm != NULL) {
        // Ensure only alarms that were inserted before the current View Alarms request are displayed
        if (current_alarm->time_stamp < view_time) {
            console_print("Alarm(%d): Type %s Group(%d) %ld %d %ld %s Status %d Assigned Display Thread %ld",
                         current_alarm->alarm_id,
                         alarm_type_to_string(current_alarm->type),
                         current_alarm->group_id,
                         (long)current_alarm->time_stamp,
                         current_alarm->interval,
                         (long)current_alarm->time,
                         current_alarm->message,
                         current_alarm->status,
                         (long)pthread_self());
        }
        current_alarm = current_alarm->link;
    }

    // Unlock the alarm list after the view operation is complete
    reader_unlock();

    // Print that View Alarms has finished processing
    console_print("View Alarms request processed and alarms listed.");

    // Unlock the mutex
    pthread_mutex_unlock(&alarm_mutex);

    return NULL;
}
