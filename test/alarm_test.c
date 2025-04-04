/**
 * @file alarm_test.c
 * @brief Test suite for functions in alarm.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "alarm.h"

/* Test helper functions */
void print_alarm(alarm_t *alarm) {
    if (alarm == NULL) {
        printf("NULL alarm\n");
        return;
    }
    
    printf("Alarm ID: %d, Group: %d, Type: %d, Status: %d, Message: %s\n",
           alarm->alarm_id, alarm->group_id, alarm->type, alarm->status, alarm->message);
}

void print_alarm_list(alarm_t *list, const char *list_name) {
    alarm_t *current = list;
    
    printf("=== %s ===\n", list_name);
    if (current == NULL) {
        printf("(empty list)\n");
        return;
    }
    
    while (current != NULL) {
        print_alarm(current);
        current = current->link;
    }
    printf("\n");
}

alarm_t *create_test_alarm(int alarm_id, int group_id, alarm_req_type_t type, 
                           alarm_status_t status, const char *message) {
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
    alarm->time = 60;  /* 1 minute */
    alarm->expiry = alarm->time_stamp + alarm->time;
    alarm->interval = 10;  /* 10 seconds */
    
    if (message != NULL) {
        strncpy(alarm->message, message, MAX_MESSAGE_LEN - 1);
        alarm->message[MAX_MESSAGE_LEN - 1] = '\0';
    } else {
        snprintf(alarm->message, MAX_MESSAGE_LEN, "Test alarm %d in group %d", 
                 alarm_id, group_id);
    }
    
    return alarm;
}

void free_alarm_list(alarm_t *list) {
    alarm_t *current = list;
    alarm_t *next;
    
    while (current != NULL) {
        next = current->link;
        free(current);
        current = next;
    }
}

/* Test insert_alarm_in_list */
void test_insert_alarm_in_list() {
    alarm_t *list = NULL;
    alarm_t *alarm1, *alarm2, *alarm3, *alarm4;
    
    printf("=== Testing insert_alarm_in_list ===\n");
    
    /* Test inserting into empty list */
    alarm1 = create_test_alarm(1, 1, REQ_START_ALARM, ALARM_ACTIVE, "First alarm");
    alarm1->time_stamp = 1000;
    insert_alarm_in_list(&list, alarm1);
    
    assert(list == alarm1);
    assert(alarm1->link == NULL);
    assert(alarm1->prev == NULL);
    printf("✓ Insert into empty list\n");
    
    /* Test inserting at the end (timestamp order) */
    alarm2 = create_test_alarm(2, 1, REQ_START_ALARM, ALARM_ACTIVE, "Second alarm");
    alarm2->time_stamp = 2000;
    insert_alarm_in_list(&list, alarm2);
    
    assert(list == alarm1);
    assert(alarm1->link == alarm2);
    assert(alarm2->prev == alarm1);
    assert(alarm2->link == NULL);
    printf("✓ Insert at end\n");
    
    /* Test inserting at the beginning */
    alarm3 = create_test_alarm(3, 2, REQ_START_ALARM, ALARM_ACTIVE, "Third alarm");
    alarm3->time_stamp = 500;
    insert_alarm_in_list(&list, alarm3);
    
    assert(list == alarm3);
    assert(alarm3->link == alarm1);
    assert(alarm1->prev == alarm3);
    printf("✓ Insert at beginning\n");
    
    /* Test inserting in the middle */
    alarm4 = create_test_alarm(4, 2, REQ_START_ALARM, ALARM_ACTIVE, "Fourth alarm");
    alarm4->time_stamp = 1500;
    insert_alarm_in_list(&list, alarm4);
    
    assert(list == alarm3);
    assert(alarm3->link == alarm1);
    assert(alarm1->link == alarm4);
    assert(alarm4->link == alarm2);
    assert(alarm4->prev == alarm1);
    assert(alarm2->prev == alarm4);
    printf("✓ Insert in middle\n");
    
    print_alarm_list(list, "Final list after insertions");
    
    /* Cleanup */
    free_alarm_list(list);
    printf("=== insert_alarm_in_list tests passed ===\n\n");
}

/* Test find_alarm_by_id */
void test_find_alarm_by_id() {
    alarm_t *list = NULL;
    alarm_t *alarm1, *alarm2, *alarm3;
    alarm_t *found;
    
    printf("=== Testing find_alarm_by_id ===\n");
    
    /* Create test alarms */
    alarm1 = create_test_alarm(101, 1, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 101");
    alarm2 = create_test_alarm(102, 1, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 102");
    alarm3 = create_test_alarm(103, 2, REQ_START_ALARM, ALARM_ACTIVE, "Alarm 103");
    
    /* Insert into list */
    insert_alarm_in_list(&list, alarm1);
    insert_alarm_in_list(&list, alarm2);
    insert_alarm_in_list(&list, alarm3);
    
    /* Find existing alarm */
    found = find_alarm_by_id(list, 102);
    assert(found == alarm2);
    printf("✓ Found existing alarm (102)\n");
    
    /* Find another existing alarm */
    found = find_alarm_by_id(list, 103);
    assert(found == alarm3);
    printf("✓ Found existing alarm (103)\n");
    
    /* Try to find non-existent alarm */
    found = find_alarm_by_id(list, 999);
    assert(found == NULL);
    printf("✓ Correctly returns NULL for non-existent alarm\n");
    
    /* Cleanup */
    free_alarm_list(list);
    printf("=== find_alarm_by_id tests passed ===\n\n");
}

/* Test get_active_group_ids with a small list */
void test_get_active_group_ids_small() {
    alarm_t *list = NULL;
    int group_ids[10];
    int count, i;
    
    printf("=== Testing get_active_group_ids (small list) ===\n");
    
    /* Empty list */
    count = get_active_group_ids(list, group_ids, 10);
    assert(count == 0);
    printf("✓ Empty list returns 0 groups\n");
    
    /* Add alarms with different groups */
    insert_alarm_in_list(&list, create_test_alarm(1, 5, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(2, 3, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(3, 5, REQ_START_ALARM, ALARM_ACTIVE, NULL));  /* Duplicate group */
    insert_alarm_in_list(&list, create_test_alarm(4, 7, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(5, 2, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    
    count = get_active_group_ids(list, group_ids, 10);
    
    /* Should have 4 unique group IDs: 2, 3, 5, 7 */
    assert(count == 4);
    printf("✓ Found %d groups\n", count);
    
    /* Check that groups are sorted */
    for (i = 0; i < count - 1; i++) {
        assert(group_ids[i] < group_ids[i+1]);
    }
    printf("✓ Groups are sorted: ");
    for (i = 0; i < count; i++) {
        printf("%d ", group_ids[i]);
    }
    printf("\n");
    
    /* Add a suspended alarm */
    insert_alarm_in_list(&list, create_test_alarm(6, 9, REQ_START_ALARM, ALARM_SUSPENDED, NULL));
    
    count = get_active_group_ids(list, group_ids, 10);
    
    /* Should have 5 unique group IDs now: 2, 3, 5, 7, 9 */
    assert(count == 5);
    printf("✓ Includes suspended alarms\n");
    
    /* Add a different type of alarm */
    insert_alarm_in_list(&list, create_test_alarm(7, 11, REQ_CHANGE_ALARM, ALARM_ACTIVE, NULL));
    
    count = get_active_group_ids(list, group_ids, 10);
    
    /* Should have 6 unique group IDs now: 2, 3, 5, 7, 9, 11 */
    assert(count == 6);
    printf("✓ Includes CHANGE alarms\n");
    
    /* Add an alarm that should be ignored (not START or CHANGE) */
    insert_alarm_in_list(&list, create_test_alarm(8, 13, REQ_CANCEL_ALARM, ALARM_ACTIVE, NULL));
    
    count = get_active_group_ids(list, group_ids, 10);
    
    /* Should still have 6 unique group IDs */
    assert(count == 6);
    printf("✓ Ignores non-START/CHANGE alarms\n");
    
    /* Add an alarm with status set to REMOVE */
    insert_alarm_in_list(&list, create_test_alarm(9, 15, REQ_START_ALARM, ALARM_REMOVE, NULL));
    
    count = get_active_group_ids(list, group_ids, 10);
    
    /* Should still have 6 unique group IDs */
    assert(count == 6);
    printf("✓ Ignores alarms with REMOVE status\n");
    
    /* Cleanup */
    free_alarm_list(list);
    printf("=== get_active_group_ids (small list) tests passed ===\n\n");
}

/* Test get_active_group_ids with a large list */
void test_get_active_group_ids_large() {
    alarm_t *list = NULL;
    int expected_unique_groups = 100;
    int *group_ids = malloc(expected_unique_groups * sizeof(int));
    int *expected_groups = malloc(expected_unique_groups * sizeof(int));
    int count, i, j, alarm_id = 1;
    
    printf("=== Testing get_active_group_ids (large list) ===\n");
    
    if (group_ids == NULL || expected_groups == NULL) {
        fprintf(stderr, "Failed to allocate memory for group arrays\n");
        exit(EXIT_FAILURE);
    }
    
    /* Create expected group IDs (100, 200, 300, ..., 10000) */
    for (i = 0; i < expected_unique_groups; i++) {
        expected_groups[i] = (i + 1) * 100;
    }
    
    /* Create 1000 alarms with 100 different group IDs */
    printf("Creating 1000 alarms with %d different group IDs...\n", expected_unique_groups);
    for (i = 0; i < 1000; i++) {
        /* Use modulo to cycle through the groups */
        int group_id = expected_groups[i % expected_unique_groups];
        
        /* Create alarm with mixed status (80% active, 20% suspended) */
        alarm_status_t status = (rand() % 5 == 0) ? ALARM_SUSPENDED : ALARM_ACTIVE;
        
        /* Create alarm with mixed type (90% START, 10% CHANGE) */
        alarm_req_type_t type = (rand() % 10 == 0) ? REQ_CHANGE_ALARM : REQ_START_ALARM;
        
        insert_alarm_in_list(&list, create_test_alarm(alarm_id++, group_id, type, status, NULL));
    }
    
    /* Add some alarms that should be ignored */
    for (i = 0; i < 100; i++) {
        /* Different request types that should be ignored */
        alarm_req_type_t type = REQ_CANCEL_ALARM + (i % 3);  /* CANCEL, SUSPEND, REACTIVATE */
        insert_alarm_in_list(&list, create_test_alarm(alarm_id++, (i + 1) * 99, type, ALARM_ACTIVE, NULL));
    }
    
    /* Add some REMOVE status alarms that should be ignored */
    for (i = 0; i < 100; i++) {
        insert_alarm_in_list(&list, create_test_alarm(alarm_id++, (i + 1) * 101, 
                             REQ_START_ALARM, ALARM_REMOVE, NULL));
    }
    
    /* Get the active group IDs */
    count = get_active_group_ids(list, group_ids, expected_unique_groups);
    
    /* Should get exactly expected_unique_groups unique IDs */
    assert(count == expected_unique_groups);
    printf("✓ Found %d unique groups\n", count);
    
    /* Check that groups are sorted */
    for (i = 0; i < count - 1; i++) {
        assert(group_ids[i] < group_ids[i+1]);
    }
    printf("✓ Groups are sorted\n");
    
    /* Check that all expected groups are present */
    for (i = 0; i < expected_unique_groups; i++) {
        int found = 0;
        for (j = 0; j < count; j++) {
            if (group_ids[j] == expected_groups[i]) {
                found = 1;
                break;
            }
        }
        assert(found);
    }
    printf("✓ All expected groups are present\n");
    
    /* Test with a limited output array */
    int small_limit = 10;
    count = get_active_group_ids(list, group_ids, small_limit);
    
    assert(count == small_limit);
    printf("✓ Respects max_groups limit (%d)\n", small_limit);
    
    /* Cleanup */
    free(group_ids);
    free(expected_groups);
    free_alarm_list(list);
    printf("=== get_active_group_ids (large list) tests passed ===\n\n");
}

/* Test is_largest_group_id */
void test_is_largest_group_id() {
    alarm_t *list = NULL;
    
    printf("=== Testing is_largest_group_id ===\n");
    
    /* Empty list */
    assert(is_largest_group_id(list, 42) == 1);
    printf("✓ Empty list: any group is considered largest\n");
    
    /* Add some alarms with different group IDs */
    insert_alarm_in_list(&list, create_test_alarm(1, 5, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(2, 3, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(3, 7, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    insert_alarm_in_list(&list, create_test_alarm(4, 2, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    
    /* Check each group ID */
    assert(is_largest_group_id(list, 2) == 0);
    assert(is_largest_group_id(list, 3) == 0);
    assert(is_largest_group_id(list, 5) == 0);
    assert(is_largest_group_id(list, 7) == 1);  /* 7 is the largest */
    assert(is_largest_group_id(list, 9) == 0);  /* Not in the list */
    
    printf("✓ Correctly identifies largest group ID\n");
    
    /* Add a larger group ID */
    insert_alarm_in_list(&list, create_test_alarm(5, 10, REQ_START_ALARM, ALARM_ACTIVE, NULL));
    
    assert(is_largest_group_id(list, 7) == 0);
    assert(is_largest_group_id(list, 10) == 1);
    
    printf("✓ Updates largest when new group is added\n");
    
    /* Cleanup */
    free_alarm_list(list);
    printf("=== is_largest_group_id tests passed ===\n\n");
}

/* Test parse_command */
void test_parse_command() {
    alarm_t alarm;
    int result;
    
    printf("=== Testing parse_command ===\n");
    
    /* Test Start_Alarm command */
    result = parse_command("Start_Alarm(123): Group(456) 30 60 This is a test message", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_START_ALARM);
    assert(alarm.alarm_id == 123);
    assert(alarm.group_id == 456);
    assert(alarm.interval == 30);
    assert(alarm.time == 60);
    assert(strcmp(alarm.message, "This is a test message") == 0);
    printf("✓ Correctly parses Start_Alarm command\n");
    
    /* Test Change_Alarm command */
    result = parse_command("Change_Alarm(456): Group(789) 120 Updated message", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_CHANGE_ALARM);
    assert(alarm.alarm_id == 456);
    assert(alarm.group_id == 789);
    assert(alarm.time == 120);
    assert(strcmp(alarm.message, "Updated message") == 0);
    printf("✓ Correctly parses Change_Alarm command\n");
    
    /* Test Cancel_Alarm command */
    result = parse_command("Cancel_Alarm(789)", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_CANCEL_ALARM);
    assert(alarm.alarm_id == 789);
    printf("✓ Correctly parses Cancel_Alarm command\n");
    
    /* Test Suspend_Alarm command */
    result = parse_command("Suspend_Alarm(123)", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_SUSPEND_ALARM);
    assert(alarm.alarm_id == 123);
    printf("✓ Correctly parses Suspend_Alarm command\n");
    
    /* Test Reactivate_Alarm command */
    result = parse_command("Reactivate_Alarm(456)", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_REACTIVATE_ALARM);
    assert(alarm.alarm_id == 456);
    printf("✓ Correctly parses Reactivate_Alarm command\n");
    
    /* Test View_Alarms command */
    result = parse_command("View_Alarms", &alarm);
    assert(result == 0);
    assert(alarm.type == REQ_VIEW_ALARMS);
    printf("✓ Correctly parses View_Alarms command\n");
    
    /* Test invalid parameters */
    result = parse_command("Start_Alarm(0): Group(456) 30 60 Invalid alarm ID", &alarm);
    assert(result == 1);
    printf("✓ Rejects invalid alarm ID (0)\n");
    
    result = parse_command("Start_Alarm(123): Group(0) 30 60 Invalid group ID", &alarm);
    assert(result == 1);
    printf("✓ Rejects invalid group ID (0)\n");
    
    result = parse_command("Start_Alarm(123): Group(456) 0 60 Invalid interval", &alarm);
    assert(result == 1);
    printf("✓ Rejects invalid interval (0)\n");
    
    result = parse_command("Start_Alarm(123): Group(456) 30 0 Invalid time", &alarm);
    assert(result == 1);
    printf("✓ Rejects invalid time (0)\n");
    
    /* Test unrecognized command */
    result = parse_command("Invalid_Command", &alarm);
    assert(result == 2);
    printf("✓ Rejects unrecognized command\n");
    
    printf("=== parse_command tests passed ===\n\n");
}

/* Main function to run all tests */
int main() {
    /* Seed the random number generator */
    srand(time(NULL));
    
    printf("Running alarm.c tests...\n\n");
    
    test_insert_alarm_in_list();
    test_find_alarm_by_id();
    test_get_active_group_ids_small();
    test_get_active_group_ids_large();
    test_is_largest_group_id();
    test_parse_command();
    
    printf("All tests passed!\n");
    return 0;
}
