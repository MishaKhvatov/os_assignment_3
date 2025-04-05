/**
 * @file change_test.c
 * @brief Test suite for change alarm thread functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include "alarm.h"
#include "display.h"
#include "errors.h"

/* Mock structures and globals */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int changes_processed;
} change_thread_data_t;

alarm_t *alarm_list = NULL;
alarm_t *change_alarm_list = NULL;
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t change_alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t change_alarm_cond = PTHREAD_COND_INITIALIZER;

sem_t write_mutex;
pthread_t change_alarm_tid;

/* Test helper functions */
alarm_t *create_test_alarm(int alarm_id, int group_id, const char *message, int interval, int alarm_time) {
    alarm_t *alarm = malloc(sizeof(alarm_t));
    if (alarm == NULL) {
        fprintf(stderr, "Failed to allocate memory for test alarm\n");
        exit(EXIT_FAILURE);
    }
    
    memset(alarm, 0, sizeof(alarm_t));
    alarm->alarm_id = alarm_id;
    alarm->group_id = group_id;
    alarm->type = REQ_START_ALARM;
    alarm->status = ALARM_ACTIVE;
    alarm->time_stamp = time(NULL);
    alarm->time = alarm_time;
    alarm->expiry = alarm->time_stamp + alarm_time;
    alarm->interval = interval;
    
    if (message) {
        strncpy(alarm->message, message, MAX_MESSAGE_LEN - 1);
        alarm->message[MAX_MESSAGE_LEN - 1] = '\0';
    } else {
        snprintf(alarm->message, MAX_MESSAGE_LEN, "Test alarm %d", alarm_id);
    }
    
    return alarm;
}

alarm_t *create_change_request(int alarm_id, int new_group_id, const char *new_message, int new_time) {
    alarm_t *change = malloc(sizeof(alarm_t));
    if (!change) {
        fprintf(stderr, "Failed to allocate memory for change alarm\n");
        exit(EXIT_FAILURE);
    }
    
    memset(change, 0, sizeof(alarm_t));
    change->alarm_id = alarm_id;
    change->group_id = new_group_id;
    change->type = REQ_CHANGE_ALARM;
    change->time_stamp = time(NULL);
    change->time = new_time;
    change->expiry = change->time_stamp + new_time;
    
    if (new_message) {
        strncpy(change->message, new_message, MAX_MESSAGE_LEN - 1);
        change->message[MAX_MESSAGE_LEN - 1] = '\0';
    }
    
    return change;
}

void cleanup_lists() {
    while (alarm_list) {
        alarm_t *next = alarm_list->link;
        free(alarm_list);
        alarm_list = next;
    }
    
    while (change_alarm_list) {
        alarm_t *next = change_alarm_list->link;
        free(change_alarm_list);
        change_alarm_list = next;
    }
}

/* Test cases */
void test_basic_alarm_change() {
    printf("=== Testing basic alarm change ===\n");
    
    /* Create and insert original alarm */
    alarm_t *original = create_test_alarm(1, 10, "Original message", 30, 60);
    insert_alarm_in_list(&alarm_list, original);
    
    /* Create change request */
    alarm_t *change = create_change_request(1, 10, "Changed message", 120);
    insert_alarm_in_list(&change_alarm_list, change);
    
    /* Signal change thread */
    pthread_cond_signal(&change_alarm_cond);
    
    /* Give thread time to process */
    usleep(100000);
    
    /* Verify changes */
    alarm_t *modified = find_alarm_by_id(alarm_list, 1);
    assert(modified != NULL);
    assert(strcmp(modified->message, change->message) == 0);
    assert(modified->time == change->time);
    assert(modified->group_id == change->group_id);
    assert(!(modified->status & ALARM_MOVED));
    
    printf("✓ Basic alarm change successful\n");
    cleanup_lists();
}

void test_group_change() {
    printf("=== Testing group change ===\n");
    
    /* Create and insert original alarm */
    alarm_t *original = create_test_alarm(1, 10, "Original message", 30, 60);
    insert_alarm_in_list(&alarm_list, original);
    
    /* Create change request with new group */
    alarm_t *change = create_change_request(1, 20, "Changed message", 60);
    insert_alarm_in_list(&change_alarm_list, change);
    
    /* Signal change thread */
    pthread_cond_signal(&change_alarm_cond);
    
    /* Give thread time to process */
    usleep(100000);
    
    /* Verify changes */
    alarm_t *modified = find_alarm_by_id(alarm_list, 1);
    assert(modified != NULL);
    assert(modified->group_id == 20);
    assert(modified->status & ALARM_MOVED);
    
    printf("✓ Group change and ALARM_MOVED flag set correctly\n");
    cleanup_lists();
}

void test_invalid_alarm_change() {
    printf("=== Testing invalid alarm change ===\n");
    
    /* Create change request for non-existent alarm */
    alarm_t *change = create_change_request(999, 10, "Changed message", 60);
    insert_alarm_in_list(&change_alarm_list, change);
    
    /* Signal change thread */
    pthread_cond_signal(&change_alarm_cond);
    
    /* Give thread time to process */
    usleep(100000);
    
    /* Verify change request was removed */
    alarm_t *found = find_alarm_by_id(change_alarm_list, 999);
    assert(found == NULL);
    
    printf("✓ Invalid change request handled correctly\n");
    cleanup_lists();
}

void test_multiple_changes() {
    printf("=== Testing multiple concurrent changes ===\n");
    
    /* Create original alarms */
    alarm_t *alarm1 = create_test_alarm(1, 10, "First alarm", 30, 60);
    alarm_t *alarm2 = create_test_alarm(2, 20, "Second alarm", 30, 60);
    insert_alarm_in_list(&alarm_list, alarm1);
    insert_alarm_in_list(&alarm_list, alarm2);
    
    /* Create multiple change requests */
    alarm_t *change1 = create_change_request(1, 15, "Changed first", 90);
    alarm_t *change2 = create_change_request(2, 25, "Changed second", 120);
    insert_alarm_in_list(&change_alarm_list, change1);
    /* Signal change thread */
    pthread_cond_signal(&change_alarm_cond);
    
    /* Give thread time to process */
    usleep(100000);

    insert_alarm_in_list(&change_alarm_list, change2);
    
    /* Signal change thread */
    pthread_cond_signal(&change_alarm_cond);
    
    /* Give thread time to process */
    usleep(100000);
    
    /* Verify both changes */
    alarm_t *modified1 = find_alarm_by_id(alarm_list, 1);
    alarm_t *modified2 = find_alarm_by_id(alarm_list, 2);
    
    assert(modified1 != NULL);
    assert(modified2 != NULL);
    assert(modified1->group_id == 15);
    assert(modified2->group_id == 25);
    assert(strcmp(modified1->message, "Changed first") == 0);
    assert(strcmp(modified2->message, "Changed second") == 0);
    
    printf("✓ Multiple changes processed correctly\n");
    cleanup_lists();
}

/* Function to implement writer lock */
void writer_lock() {
    sem_wait(&write_mutex);
}

/* Function to implement writer unlock */
void writer_unlock() {
    sem_post(&write_mutex);
}


/* THREADS */
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
            errno_abort("Failed to lock change alarm mutex");
        }
        writer_lock();
        /* The alarm(s) in change_alarm_list will be processed */
        change_alarm = change_alarm_list;
        while (change_alarm != NULL) {
            if (alarm_list == NULL) {
                printf("NO ALARM IN ALARM LIST\n");
            }
            alarm_t *alarm = find_alarm_by_id(alarm_list, change_alarm->alarm_id);
            if (alarm == NULL) {
                /* Handle invalid change request */
                printf("Invalid Change Alarm Request(%d at %ld: Group(%d) %ld %ld %s\n",
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

                printf("Change Alarm Thread %ld Has Changed Alarm(%d at %ld: Group(%d) <%ld %d %ld %s)\n",
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
            errno_abort("Failed to unlock change alarm mutex");
        }
        
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0) {
            errno_abort("Failed to unlock alarm mutex");
        }

    }
    return NULL;
}

void make_workers() {
    int status;

    status = pthread_create(&change_alarm_tid, NULL, change_alarm_thread, NULL);
    if (status != 0) {
        errno_abort("Failed to create change alarm thread");
    }

}

int main() {
    printf("Starting change alarm thread tests...\n\n");

    make_workers();
    test_basic_alarm_change();
    test_group_change();
    test_invalid_alarm_change();
    test_multiple_changes();
    
    printf("\nAll change alarm thread tests passed!\n");
    return 0;
}
