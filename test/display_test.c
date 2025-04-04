/**
 * @file display_test.c
 * @brief Test file for display.c and display thread functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include "alarm.h"
#include "display.h"
#include "errors.h"

/* Define a buffer size for our mock console output */
#define MOCK_BUFFER_SIZE 4096
#define MAX_OUTPUT_LINES 100
#define MAX_LINE_LENGTH 256

/* Global variables for mock console output */
char mock_output_buffer[MOCK_BUFFER_SIZE];
char output_lines[MAX_OUTPUT_LINES][MAX_LINE_LENGTH];
int num_output_lines = 0;
pthread_mutex_t mock_console_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Mock alarm list for testing */
alarm_t *test_alarm_list = NULL;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
int most_recent_displayed_alarm_id = -1;

/* External declarations for functions in alarm.c */
extern void insert_alarm_in_list(alarm_t **list_head, alarm_t *alarm);
extern alarm_t *find_alarm_by_id(alarm_t *list, int alarm_id);
extern int get_active_group_ids(alarm_t *list, int *group_ids, int max_groups);
extern int is_largest_group_id(alarm_t *list, int group_id);

/* Reader-Writer semaphores for alarm list */
sem_t read_count_mutex;
sem_t alarm_list_mutex_sem;
sem_t write_mutex;
int read_count = 0;

/* Forward declarations */
void reader_lock();
void reader_unlock();
void writer_lock();
void writer_unlock();
int is_next_group_to_display(int group_id);
display_thread_t *create_display_thread(int group_id, alarm_t *alarm);
void* display_alarm_thread(void* arg);

/* Mock console_print function */
void console_print(const char *format, ...) {
    va_list args;
    char temp_buffer[MAX_LINE_LENGTH];
    
    va_start(args, format);
    vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);
    
    pthread_mutex_lock(&mock_console_mutex);
    
    /* Append to the main buffer */
    size_t current_len = strlen(mock_output_buffer);
    if (current_len + strlen(temp_buffer) + 2 < MOCK_BUFFER_SIZE) {
        strcat(mock_output_buffer, temp_buffer);
        strcat(mock_output_buffer, "\n");
    }
    
    /* Add to the lines array for easier testing */
    if (num_output_lines < MAX_OUTPUT_LINES) {
        strncpy(output_lines[num_output_lines], temp_buffer, MAX_LINE_LENGTH - 1);
        output_lines[num_output_lines][MAX_LINE_LENGTH - 1] = '\0';
        num_output_lines++;
    }
    
    /* Print to stdout for debugging */
    printf("MOCK: %s\n", temp_buffer);
    
    pthread_mutex_unlock(&mock_console_mutex);
}

/* Reset the mock console output */
void reset_mock_console() {
    pthread_mutex_lock(&mock_console_mutex);
    mock_output_buffer[0] = '\0';
    num_output_lines = 0;
    memset(output_lines, 0, sizeof(output_lines));
    pthread_mutex_unlock(&mock_console_mutex);
}

/* Helper function to check if a particular string is present in the output */
int output_contains(const char *str) {
    pthread_mutex_lock(&mock_console_mutex);
    int found = (strstr(mock_output_buffer, str) != NULL);
    pthread_mutex_unlock(&mock_console_mutex);
    return found;
}

/* Implementations of reader/writer locks */
void reader_lock() {
    sem_wait(&read_count_mutex);
    read_count++;
    if (read_count == 1) {
        sem_wait(&write_mutex);
    }
    sem_post(&read_count_mutex);
    sem_wait(&alarm_list_mutex_sem);
}

void reader_unlock() {
    sem_post(&alarm_list_mutex_sem);
    sem_wait(&read_count_mutex);
    read_count--;
    if (read_count == 0) {
        sem_post(&write_mutex);
    }
    sem_post(&read_count_mutex);
}

void writer_lock() {
    sem_wait(&write_mutex);
}

void writer_unlock() {
    sem_post(&write_mutex);
}

/* Implementation of is_next_group_to_display for round-robin scheduling */
int is_next_group_to_display(int group_id) {
    int group_ids[100]; /* Reasonably large array for group IDs */
    int count, i, last_displayed_idx = -1;
    alarm_t *current;
    int last_group_id = -1;

    /* Get all unique active group IDs, sorted in ascending order */
    reader_lock();
    count = get_active_group_ids(test_alarm_list, group_ids, 100);
    reader_unlock();
    
    if (count == 0) return 1; /* No other groups, so yes */
    if (count == 1) return (group_ids[0] == group_id); /* Only one group */

    /* Find the group ID of the most recently displayed alarm */
    if (most_recent_displayed_alarm_id >= 0) {
        reader_lock();
        current = test_alarm_list;
        while (current != NULL) {
            if (current->alarm_id == most_recent_displayed_alarm_id) {
                last_group_id = current->group_id;
                break;
            }
            current = current->link;
        }
        reader_unlock();

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

/* Test helper functions */
alarm_t *create_test_alarm(int alarm_id, int group_id, alarm_req_type_t type, 
                           alarm_status_t status, const char *message, int interval, int time_val) {
    alarm_t *alarm = malloc(sizeof(alarm_t));
    if (alarm == NULL) {
        fprintf(stderr, "Failed to allocate memory for test alarm\n");
        exit(EXIT_FAILURE);
    }
    
    memset(alarm, 0, sizeof(alarm_t));
    alarm->alarm_id = alarm_id;
    alarm->group_id = group_id;
    alarm->type = type;
    alarm->status = status;
    alarm->time_stamp = time(NULL);
    alarm->time = time_val;
    alarm->expiry = alarm->time_stamp + alarm->time;
    alarm->interval = interval;
    
    if (message != NULL) {
        strncpy(alarm->message, message, MAX_MESSAGE_LEN - 1);
        alarm->message[MAX_MESSAGE_LEN - 1] = '\0';
    } else {
        snprintf(alarm->message, MAX_MESSAGE_LEN, "Test alarm %d in group %d", 
                 alarm_id, group_id);
    }
    
    return alarm;
}

void cleanup_test_data() {
    /* Free all alarms in the list */
    alarm_t *current = test_alarm_list;
    alarm_t *next;
    
    writer_lock();
    while (current != NULL) {
        next = current->link;
        free(current);
        current = next;
    }
    test_alarm_list = NULL;
    writer_unlock();
    
    reset_mock_console();
}

void print_alarm_list(alarm_t *list) {
    alarm_t *current = list;
    printf("\n=== Current Alarm List ===\n");
    if (current == NULL) {
        printf("(empty list)\n");
        return;
    }
    
    while (current != NULL) {
        printf("Alarm ID: %d, Group: %d, Type: %d, Status: %d, Message: %s\n",
               current->alarm_id, current->group_id, current->type, 
               current->status, current->message);
        current = current->link;
    }
    printf("\n");
}

/* Mock implementation of the display_alarm_thread function */
void* display_alarm_thread(void* arg) {
    display_thread_t *dt = (display_thread_t*) arg;

    if (dt == NULL) {
        return NULL;
    }

    alarm_snapshot_t *snapshot_1 = NULL;
    alarm_snapshot_t *snapshot_2 = NULL;
    time_t current_time;
    

    /* Initialize the snapshots if alarms are already present */
    pthread_mutex_lock(&dt->mutex);
    if (dt->alarm_1 != NULL) {
        snapshot_1 = malloc(sizeof(alarm_snapshot_t));
        if (snapshot_1 == NULL) {
            pthread_mutex_unlock(&dt->mutex);
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
        current_time = time(NULL);

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
        pthread_mutex_lock(&dt->mutex);
        reader_unlock();

        /* Logic for checking round-robin synchronization */
        if (!is_next_group_to_display(dt->group_id)) {
            pthread_mutex_unlock(&dt->mutex);
            reader_unlock();
            continue;
        }

        if(snapshot_1 != NULL){
            update_snapshot(snapshot_1, dt->alarm_1, dt->thread_id);
            periodic_print(snapshot_1, dt->thread_id);
            /* Mark this thread's alarms as most recently displayed */
            pthread_mutex_lock(&display_mutex);
            most_recent_displayed_alarm_id = snapshot_1->alarm_id;

            /* If this is the largest group ID, reset for next cycle */
            if (is_largest_group_id(test_alarm_list, dt->group_id)) {
                most_recent_displayed_alarm_id = -1;
            }
            pthread_mutex_unlock(&display_mutex);
    
            if (snapshot_1->status == ALARM_REMOVE) {
                free(snapshot_1);
                snapshot_1 = NULL;
                dt->alarm_count--;
                dt->alarm_1 = NULL;
            }
        }
        
        if(snapshot_2 != NULL){
            update_snapshot(snapshot_2, dt->alarm_2, dt->thread_id);
            periodic_print(snapshot_2, dt->thread_id);
            /* Mark this thread's alarms as most recently displayed */
            pthread_mutex_lock(&display_mutex);
            most_recent_displayed_alarm_id = snapshot_2->alarm_id;
            /* If this is the largest group ID, reset for next cycle */
            if (is_largest_group_id(test_alarm_list, dt->group_id)) {
                most_recent_displayed_alarm_id = -1;
            }
            pthread_mutex_unlock(&display_mutex);

            if (snapshot_2->status == ALARM_REMOVE) {
                free(snapshot_2);
                snapshot_2 = NULL;
                dt->alarm_count--;
                dt->alarm_2 = NULL; 
            }
        }
        reader_unlock();
        pthread_mutex_unlock(&dt->mutex);
    }
    
    /* Clean up snapshots if test ended before natural termination */
    if (snapshot_1) free(snapshot_1);
    if (snapshot_2) free(snapshot_2);
    
    return NULL;
}

/* Implementation of create_display_thread for testing */
display_thread_t *create_display_thread(int group_id, alarm_t *alarm) {
    display_thread_t *new_dt;
    int status;

    if (alarm == NULL) {
        return NULL;
    }

    new_dt = (display_thread_t *)malloc(sizeof(display_thread_t));
    if (new_dt == NULL) {
        return NULL;
    }

    /* Initialize the display thread */
    memset(new_dt, 0, sizeof(display_thread_t));
    new_dt->group_id = group_id;
    new_dt->alarm_1 = alarm;  // Assign the initial alarm
    new_dt->alarm_count = 1;  // Set initial alarm count
    new_dt->next = NULL;
    
    status = pthread_mutex_init(&new_dt->mutex, NULL);
    if (status != 0) {
        free(new_dt);
        return NULL;
    }

    /* Create the thread */
    status = pthread_create(&new_dt->thread_id, NULL, display_alarm_thread, (void *)new_dt);
    if (status != 0) {
        pthread_mutex_destroy(&new_dt->mutex);
        free(new_dt);
        return NULL;
    }
    
    console_print("New Display Alarm Thread %ld Created for Group(%d) at %ld",
        (long)new_dt->thread_id, new_dt->group_id, (long)time(NULL));

    return new_dt;
}

/* Test functions for display.c functions */

void test_create_snapshot() {
    printf("\n=== Testing create_snapshot ===\n");
    
    alarm_t *alarm = create_test_alarm(1, 10, REQ_START_ALARM, ALARM_ACTIVE, "Test message", 5, 60);
    alarm_snapshot_t snapshot;
    
    memset(&snapshot, 0, sizeof(alarm_snapshot_t));
    
    /* Set a flag in the alarm status to test it gets copied correctly */
    alarm->status = ALARM_ACTIVE | ALARM_MOVED;
    
    create_snapshot(&snapshot, alarm);
    
    assert(snapshot.alarm_id == alarm->alarm_id);
    assert(snapshot.group_id == alarm->group_id);
    assert(snapshot.status == ALARM_ACTIVE); /* ALARM_MOVED flag should be removed */
    assert(snapshot.interval == alarm->interval);
    assert(snapshot.time == alarm->time);
    assert(strcmp(snapshot.message, alarm->message) == 0);
    printf("✓ Snapshot correctly created with all fields copied\n");
    
    /* Test with NULL alarm */
    memset(&snapshot, 0, sizeof(alarm_snapshot_t));
    create_snapshot(&snapshot, NULL);
    /* Should not crash and snapshot should remain zeroed */
    assert(snapshot.alarm_id == 0);
    assert(snapshot.group_id == 0);
    assert(snapshot.status == 0);
    printf("✓ Handles NULL alarm gracefully\n");
    
    /* Test with NULL snapshot */
    create_snapshot(NULL, alarm);
    /* Should not crash */
    printf("✓ Handles NULL snapshot gracefully\n");
    
    free(alarm);
    printf("=== create_snapshot tests passed ===\n");
}

void test_update_snapshot() {
    printf("\n=== Testing update_snapshot ===\n");
    reset_mock_console();
    
    alarm_t *alarm = create_test_alarm(1, 10, REQ_START_ALARM, ALARM_ACTIVE, "Test message", 5, 60);
    alarm_snapshot_t snapshot;
    pthread_t thread_id = pthread_self();
    
    /* Initialize snapshot */
    memset(&snapshot, 0, sizeof(alarm_snapshot_t));
    create_snapshot(&snapshot, alarm);
    
    /* Test 1: Normal update, no changes */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Test message") == 0); /* No message should be printed */
    printf("✓ No message printed when no changes detected\n");
    
    /* Test 2: Alarm is NULL (removed) */
    reset_mock_console();
    update_snapshot(&snapshot, NULL, thread_id);
    assert(output_contains("Display Thread") == 1); 
    assert(output_contains("Has Stopped Printing Message of Alarm(1)") == 1);
    assert(snapshot.status == ALARM_REMOVE);
    printf("✓ Correctly handles NULL alarm (removed)\n");
    
    /* Test 3: Alarm is expired */
    reset_mock_console();
    snapshot.status = ALARM_ACTIVE; /* Reset status */
    alarm->expiry = time(NULL) - 10; /* Set to expired (past time) */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Has Stopped Printing Expired Alarm(1)") == 1);
    assert(snapshot.status == ALARM_REMOVE);
    printf("✓ Correctly handles expired alarm\n");
    
    /* Test 4: Group ID changed */
    reset_mock_console();
    snapshot.status = ALARM_ACTIVE; /* Reset status */
    alarm->expiry = time(NULL) + 60; /* Reset expiry */
    alarm->group_id = 20; /* Changed group ID */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Has Stopped Printing Message of Alarm(1)") == 1);
    assert(snapshot.status == ALARM_REMOVE);
    printf("✓ Correctly handles group ID change\n");
    
    /* Test 5: Alarm moved to new group (for new display thread) */
    reset_mock_console();
    snapshot.status = ALARM_ACTIVE; /* Reset status */
    snapshot.group_id = 20; /* Make snapshot match new group ID */
    alarm->status = ALARM_ACTIVE | ALARM_MOVED; /* Set moved flag */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Has Taken Over Printing Message of Alarm(1)") == 1);
    assert(snapshot.status == ALARM_MOVED);
    printf("✓ Correctly handles alarm moved to new group\n");
    
    /* Test 6: Message changed */
    reset_mock_console();
    snapshot.status = ALARM_ACTIVE; /* Reset status */
    strncpy(alarm->message, "Changed message", MAX_MESSAGE_LEN);
    alarm->status = ALARM_ACTIVE; /* Remove moved flag */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Starts to Print Changed Message Alarm(1)") == 1);
    assert(strcmp(snapshot.message, "Changed message") == 0);
    printf("✓ Correctly handles message change\n");
    
    /* Test 7: Interval changed */
    reset_mock_console();
    alarm->interval = 10; /* Change interval */
    update_snapshot(&snapshot, alarm, thread_id);
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Starts to Print Changed Interval Value Alarm(1)") == 1);
    assert(snapshot.interval == 10);
    printf("✓ Correctly handles interval change\n");
    
    free(alarm);
    printf("=== update_snapshot tests passed ===\n");
}

void test_periodic_print() {
    printf("\n=== Testing periodic_print ===\n");
    reset_mock_console();
    
    alarm_t *alarm = create_test_alarm(1, 10, REQ_START_ALARM, ALARM_ACTIVE, "Test message", 5, 60);
    alarm_snapshot_t snapshot;
    pthread_t thread_id = pthread_self();
    
    /* Initialize snapshot */
    memset(&snapshot, 0, sizeof(alarm_snapshot_t));
    create_snapshot(&snapshot, alarm);
    
    /* Test 1: Suspended alarm, should not print */
    snapshot.status = ALARM_SUSPENDED;
    periodic_print(&snapshot, thread_id);
    assert(output_contains("Alarm (1) Printed") == 0);
    printf("✓ Doesn't print suspended alarms\n");
    
    /* Test 2: Removed alarm, should not print */
    reset_mock_console();
    snapshot.status = ALARM_REMOVE;
    periodic_print(&snapshot, thread_id);
    assert(output_contains("Alarm (1) Printed") == 0);
    printf("✓ Doesn't print removed alarms\n");
    
    /* Test 3: Active alarm, first print */
    reset_mock_console();
    snapshot.status = ALARM_ACTIVE;
    snapshot.last_print_time = 0; /* Force first print */
    periodic_print(&snapshot, thread_id);
    assert(output_contains("Alarm (1) Printed by Alarm Display Thread") == 1);
    printf("✓ Prints active alarms correctly\n");
    
    /* Test 4: Active alarm, too soon for next print */
    reset_mock_console();
    snapshot.last_print_time = time(NULL); /* Just printed */
    periodic_print(&snapshot, thread_id);
    assert(output_contains("Alarm (1) Printed") == 0);
    printf("✓ Doesn't print when interval hasn't elapsed\n");
    
    /* Test 5: Active alarm, interval elapsed */
    reset_mock_console();
    snapshot.last_print_time = time(NULL) - 6; /* Last printed 6 seconds ago, interval is 5 */
    periodic_print(&snapshot, thread_id);
    assert(output_contains("Alarm (1) Printed by Alarm Display Thread") == 1);
    printf("✓ Prints when interval has elapsed\n");
    
    free(alarm);
    printf("=== periodic_print tests passed ===\n");
}

void test_display_thread_single() {
    printf("\n=== Testing display thread with a single alarm ===\n");
    reset_mock_console();
    cleanup_test_data();
    
    /* Create a test alarm and add it to our test list */
    alarm_t *alarm = create_test_alarm(101, 10, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 101 message", 2, 30);
    
    writer_lock();
    insert_alarm_in_list(&test_alarm_list, alarm);
    writer_unlock();
    
    /* Create a display thread for this alarm */
    display_thread_t *dt = create_display_thread(10, alarm);
    assert(dt != NULL);
    
    /* Let the display thread run for a short time */
    sleep(1);
    reset_mock_console();
    sleep(5);
    
    /* Check that periodic prints happened */
    assert(output_contains("Alarm (101) Printed by Alarm Display Thread") == 1);
    printf("✓ Display thread correctly prints alarm messages periodically\n");
    
    /* Test changing the message */
    writer_lock();
    strncpy(alarm->message, "Updated message for alarm 101", MAX_MESSAGE_LEN - 1);
    writer_unlock();
    
    sleep(3);
    
    /* Check that the changed message was detected and printed */
    assert(output_contains("Starts to Print Changed Message Alarm(101)") == 1);
    assert(output_contains("Updated message for alarm 101") == 1);
    printf("✓ Display thread correctly detects and handles message changes\n");
    
    /* Test changing the interval */
    writer_lock();
    alarm->interval = 5;
    writer_unlock();
    
    sleep(3);
    
    /* Check that the changed interval was detected */
    assert(output_contains("Starts to Print Changed Interval Value Alarm(101)") == 1);
    printf("✓ Display thread correctly detects and handles interval changes\n");
    
    /* Test suspending the alarm */
    writer_lock();
    alarm->status = ALARM_SUSPENDED;
    writer_unlock();
    
    sleep(5);
    
    /* Check that printing stopped (we capture the last print time before and after) */
    reset_mock_console();
    sleep(3);
    assert(output_contains("Alarm (101) Printed") == 0);
    printf("✓ Display thread correctly stops printing for suspended alarms\n");
    
    /* Test reactivating the alarm */
    writer_lock();
    alarm->status = ALARM_ACTIVE;
    writer_unlock();
    
    sleep(6);
    
    /* Check that printing resumed */
    assert(output_contains("Alarm (101) Printed") == 1);
    printf("✓ Display thread correctly resumes printing for reactivated alarms\n");
    
    /* Test canceling the alarm */
    reset_mock_console();
    /* Simulate alarm removal by setting to NULL in the display thread */
    pthread_mutex_lock(&dt->mutex);
    dt->alarm_1 = NULL;
    dt->alarm_count = 0;
    pthread_mutex_unlock(&dt->mutex);
    
    sleep(3);
    
    assert(output_contains("No More Alarms in Group(10): Display Thread") == 1);
    assert(output_contains("exiting at") == 1);
    printf("✓ Display thread correctly exits when all alarms are removed\n");
    
    /* Clean up */
    pthread_join(dt->thread_id, NULL);
    pthread_mutex_destroy(&dt->mutex);
    free(dt);
    
    writer_lock();
    free(alarm);
    test_alarm_list = NULL;
    writer_unlock();
    
    printf("=== Display thread single alarm tests passed ===\n");
}

void test_display_thread_multiple() {
    printf("\n=== Testing display thread with multiple alarms ===\n");
    reset_mock_console();
    cleanup_test_data();
    
    /* Create test alarms with different group IDs */
    alarm_t *alarm1 = create_test_alarm(201, 20, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 201 group 20", 2, 60);
    alarm_t *alarm2 = create_test_alarm(202, 30, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 202 group 30", 2, 60);
    alarm_t *alarm3 = create_test_alarm(203, 20, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 203 group 20", 2, 60);
    
    writer_lock();
    insert_alarm_in_list(&test_alarm_list, alarm1);
    insert_alarm_in_list(&test_alarm_list, alarm2);
    insert_alarm_in_list(&test_alarm_list, alarm3);
    writer_unlock();
    
    /* Create display threads for these groups */
    display_thread_t *dt1 = create_display_thread(20, alarm1);
    display_thread_t *dt2 = create_display_thread(30, alarm2);
    
    /* Assign second alarm to first display thread */
    pthread_mutex_lock(&dt1->mutex);
    dt1->alarm_2 = alarm3;
    dt1->alarm_count = 2;
    pthread_mutex_unlock(&dt1->mutex);
    
    /* Let the display threads run for a short time */
    sleep(8);
    
    /* Verify that alarms from both groups are being displayed */
    assert(output_contains("Alarm (201) Printed by Alarm Display Thread") == 1);
    assert(output_contains("Alarm (202) Printed by Alarm Display Thread") == 1);
    assert(output_contains("Alarm (203) Printed by Alarm Display Thread") == 1);
    printf("✓ Multiple display threads print alarms from different groups\n");
    
    /* Test the group ID change mechanism */
    reset_mock_console();
    
    /* Change the group ID of an alarm */
    writer_lock();
    alarm1->group_id = 30;  /* Move from group 20 to group 30 */
    writer_unlock();
    
    /* Signal threads to check for updates */
    sleep(3);
    
    /* Check that the original thread detected the removal */
    assert(output_contains("Display Thread") == 1);
    assert(output_contains("Has Stopped Printing Message of Alarm(201)") == 1);
    
    pthread_mutex_lock(&dt2->mutex);
    alarm1->status |= ALARM_MOVED;  /* Set the moved flag */
    dt2->alarm_2 = alarm1;
    pthread_mutex_unlock(&dt2->mutex);
    
    sleep(3);
    
    /* Check that the new thread took over */
    assert(output_contains("Has Taken Over Printing Message of Alarm(201)") == 1);
    printf("✓ Display threads correctly handle alarm group changes\n");
    
    /* Test expiry mechanism */
    reset_mock_console();
    
    /* Set an alarm to expire */
    writer_lock();
    alarm2->expiry = time(NULL) - 5;  /* Set to expired (past time) */
    writer_unlock();
    
    sleep(3);
    
    /* Check that the thread detected the expiry */
    assert(output_contains("Has Stopped Printing Expired Alarm(202)") == 1);
    printf("✓ Display threads correctly handle expired alarms\n");
    
    printf("=== Display thread multiple alarms tests passed ===\n");
}
/* Main function to run all tests */
int main() {
    /* Initialize semaphores */
    sem_init(&read_count_mutex, 0, 1);
    sem_init(&alarm_list_mutex_sem, 0, 1);
    sem_init(&write_mutex, 0, 1);
    
    printf("Running display.c and display thread tests...\n\n");
    
    /* Test the basic display.c functions */
    test_create_snapshot();
    test_update_snapshot();
    test_periodic_print();
    
    /* Test the display thread functionality */
    test_display_thread_single();
    test_display_thread_multiple();
    
    /* Clean up */
    sem_destroy(&read_count_mutex);
    sem_destroy(&alarm_list_mutex_sem);
    sem_destroy(&write_mutex);
    
    printf("\nAll tests passed!\n");
    return 0;
}
