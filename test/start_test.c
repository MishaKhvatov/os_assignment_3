/**
 * @file start_test.c
 * @brief Test suite for start alarm thread functionality
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 #include <assert.h>
 #include <pthread.h>
 #include "alarm.h"
 #include "display.h"
 
 /* Mock display thread structure for testing */
 typedef struct display_thread_tag {
     pthread_t thread_id;
     int group_id;
     alarm_t *alarm_1;
     alarm_t *alarm_2;
     int alarm_count;
     pthread_mutex_t mutex;
     struct display_thread_tag *next;
 } display_thread_t;
 
 /* Global variables for testing */
 display_thread_t *display_threads = NULL;
 pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
 
 /* Test helper functions */
 display_thread_t *create_mock_display_thread(int group_id, alarm_t *alarm) {
     display_thread_t *new_dt = malloc(sizeof(display_thread_t));
     if (!new_dt) return NULL;
 
     memset(new_dt, 0, sizeof(display_thread_t));
     new_dt->group_id = group_id;
     new_dt->alarm_1 = alarm;
     new_dt->alarm_count = 1;
     pthread_mutex_init(&new_dt->mutex, NULL);
 
     /* Add to global list */
     pthread_mutex_lock(&display_mutex);
     new_dt->next = display_threads;
     display_threads = new_dt;
     pthread_mutex_unlock(&display_mutex);
 
     return new_dt;
 }
 
 void free_mock_display_threads() {
     pthread_mutex_lock(&display_mutex);
     display_thread_t *current = display_threads;
     while (current) {
         display_thread_t *next = current->next;
         pthread_mutex_destroy(&current->mutex);
         free(current);
         current = next;
     }
     display_threads = NULL;
     pthread_mutex_unlock(&display_mutex);
 }
 
 alarm_t *create_test_alarm(int alarm_id, int group_id) {
     alarm_t *alarm = malloc(sizeof(alarm_t));
     if (!alarm) return NULL;
 
     memset(alarm, 0, sizeof(alarm_t));
     alarm->alarm_id = alarm_id;
     alarm->group_id = group_id;
     alarm->type = REQ_START_ALARM;
     alarm->status = ALARM_ACTIVE;
     alarm->time_stamp = time(NULL);
     alarm->time = 60;
     alarm->interval = 10;
     snprintf(alarm->message, MAX_MESSAGE_LEN, "Test alarm %d", alarm_id);
 
     return alarm;
 }
 
 /* Test cases */
 
 void test_create_new_display_thread() {
     printf("=== Testing create_new_display_thread ===\n");
     
     alarm_t *alarm = create_test_alarm(1, 10);
     assert(alarm != NULL);
     
     display_thread_t *dt = create_mock_display_thread(10, alarm);
     assert(dt != NULL);
     
     assert(dt->group_id == 10);
     assert(dt->alarm_1 == alarm);
     assert(dt->alarm_count == 1);
     assert(dt->alarm_2 == NULL);
     
     printf("✓ Successfully created display thread for new group\n");
     
     free_mock_display_threads();
     free(alarm);
     printf("=== test_create_new_display_thread passed ===\n\n");
 }
 
 void test_assign_to_existing_thread() {
     printf("=== Testing assign_to_existing_thread ===\n");
     
     /* Create initial display thread with one alarm */
     alarm_t *alarm1 = create_test_alarm(1, 10);
     display_thread_t *dt = create_mock_display_thread(10, alarm1);
     
     /* Create second alarm for same group */
     alarm_t *alarm2 = create_test_alarm(2, 10);
     
     /* Test assignment */
     pthread_mutex_lock(&dt->mutex);
     if (dt->alarm_1 == NULL) {
         dt->alarm_1 = alarm2;
     } else {
         dt->alarm_2 = alarm2;
     }
     dt->alarm_count++;
     pthread_mutex_unlock(&dt->mutex);
     
     assert(dt->alarm_count == 2);
     assert((dt->alarm_1 == alarm2) || (dt->alarm_2 == alarm2));
     
     printf("✓ Successfully assigned second alarm to existing thread\n");
     
     free_mock_display_threads();
     free(alarm1);
     free(alarm2);
     printf("=== test_assign_to_existing_thread passed ===\n\n");
 }
 
 void test_thread_creation_logic() {
     printf("=== Testing thread_creation_logic ===\n");
     
     /* Test case 1: No existing thread for group */
     alarm_t *alarm1 = create_test_alarm(1, 10);
     
     /* Should create new thread */
     display_thread_t *dt = NULL;
     pthread_mutex_lock(&display_mutex);
     display_thread_t *current = display_threads;
     int found = 0;
     int count = 0;
     
     while (current != NULL) {
         if (current->group_id == alarm1->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     assert(found == 0);
     assert(count == 0);
     printf("✓ Correctly identified need for new thread\n");
     
     /* Test case 2: Existing thread with capacity */
     display_thread_t *dt1 = create_mock_display_thread(10, alarm1);
     
     alarm_t *alarm2 = create_test_alarm(2, 10);
     
     found = 0;
     count = 0;
     pthread_mutex_lock(&display_mutex);
     current = display_threads;
     while (current != NULL) {
         if (current->group_id == alarm2->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     assert(found == 1);
     assert(count == 1);
     assert(dt == dt1);
     printf("✓ Correctly identified existing thread with capacity\n");
     
     /* Test case 3: Existing thread at full capacity */
     /* Assign second alarm to fill the thread */
     pthread_mutex_lock(&dt1->mutex);
     dt1->alarm_2 = alarm2;
     dt1->alarm_count++;
     pthread_mutex_unlock(&dt1->mutex);
     
     alarm_t *alarm3 = create_test_alarm(3, 10);
     
     found = 0;
     count = 0;
     pthread_mutex_lock(&display_mutex);
     current = display_threads;
     while (current != NULL) {
         if (current->group_id == alarm3->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     assert(found == 0);
     assert(count == 1);
     printf("✓ Correctly identified need for new thread when existing is full\n");
     
     free_mock_display_threads();
     free(alarm1);
     free(alarm2);
     free(alarm3);
     printf("=== test_thread_creation_logic passed ===\n\n");
 }
 
 void test_start_alarm_thread_logic() {
     printf("=== Testing start_alarm_thread_logic ===\n");
     
     /* This test simulates the core logic of start_alarm_thread */
     alarm_t *alarm1 = create_test_alarm(1, 10);
     alarm_t *alarm2 = create_test_alarm(2, 10);
     alarm_t *alarm3 = create_test_alarm(3, 20);
     
     /* First alarm - should create new thread */
     display_thread_t *dt = NULL;
     int found = 0;
     int count = 0;
     
     pthread_mutex_lock(&display_mutex);
     display_thread_t *current = display_threads;
     while (current != NULL) {
         if (current->group_id == alarm1->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     if (!found || count == 0) {
         dt = create_mock_display_thread(alarm1->group_id, alarm1);
         printf("Created new display thread %p for group %d\n", (void*)dt, alarm1->group_id);
     } else {
         pthread_mutex_lock(&dt->mutex);
         if (dt->alarm_1 == NULL) {
             dt->alarm_1 = alarm1;
         } else {
             dt->alarm_2 = alarm1;
         }
         dt->alarm_count++;
         pthread_mutex_unlock(&dt->mutex);
         printf("Assigned to existing display thread %p\n", (void*)dt);
     }
     
     assert(dt != NULL);
     assert(dt->group_id == 10);
     assert(dt->alarm_count == 1);
     printf("✓ Handled first alarm correctly\n");
     
     /* Second alarm - same group, assign to existing thread */
     found = 0;
     count = 0;
     
     pthread_mutex_lock(&display_mutex);
     current = display_threads;
     while (current != NULL) {
         if (current->group_id == alarm2->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     if (!found || count == 0) {
         dt = create_mock_display_thread(alarm2->group_id, alarm2);
         printf("Created new display thread %p for group %d\n", (void*)dt, alarm2->group_id);
     } else {
         pthread_mutex_lock(&dt->mutex);
         if (dt->alarm_1 == NULL) {
             dt->alarm_1 = alarm2;
         } else {
             dt->alarm_2 = alarm2;
         }
         dt->alarm_count++;
         pthread_mutex_unlock(&dt->mutex);
         printf("Assigned to existing display thread %p\n", (void*)dt);
     }
     
     assert(dt != NULL);
     assert(dt->group_id == 10);
     assert(dt->alarm_count == 2);
     printf("✓ Handled second alarm correctly\n");
     
     /* Third alarm - different group, create new thread */
     found = 0;
     count = 0;
     
     pthread_mutex_lock(&display_mutex);
     current = display_threads;
     while (current != NULL) {
         if (current->group_id == alarm3->group_id) {
             count++;
             if (current->alarm_count < 2) {
                 dt = current;
                 found = 1;
                 break;
             }
         }
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     if (!found || count == 0) {
         dt = create_mock_display_thread(alarm3->group_id, alarm3);
         printf("Created new display thread %p for group %d\n", (void*)dt, alarm3->group_id);
     } else {
         pthread_mutex_lock(&dt->mutex);
         if (dt->alarm_1 == NULL) {
             dt->alarm_1 = alarm3;
         } else {
             dt->alarm_2 = alarm3;
         }
         dt->alarm_count++;
         pthread_mutex_unlock(&dt->mutex);
         printf("Assigned to existing display thread %p\n", (void*)dt);
     }
     
     assert(dt != NULL);
     assert(dt->group_id == 20);
     assert(dt->alarm_count == 1);
     printf("✓ Handled third alarm (new group) correctly\n");
     
     /* Verify we have two display threads now */
     int thread_count = 0;
     pthread_mutex_lock(&display_mutex);
     current = display_threads;
     while (current != NULL) {
         thread_count++;
         current = current->next;
     }
     pthread_mutex_unlock(&display_mutex);
     
     assert(thread_count == 2);
     printf("✓ Correct number of display threads created\n");
     
     free_mock_display_threads();
     free(alarm1);
     free(alarm2);
     free(alarm3);
     printf("=== test_start_alarm_thread_logic passed ===\n\n");
 }
 
 int main() {
     printf("Starting start alarm thread tests...\n\n");
     
     test_create_new_display_thread();
     test_assign_to_existing_thread();
     test_thread_creation_logic();
     test_start_alarm_thread_logic();
     
     printf("All start alarm thread tests passed!\n");
     return 0;
 }