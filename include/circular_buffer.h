/**
 * @file circular_buffer.h
 * @brief Circular buffer implementation for alarm system
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "alarm.h"

/**
 * @brief Circular buffer structure
 */
typedef struct {
    alarm_t *alarms[CIRCULAR_BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} circular_buffer_t;
/**
 * @brief Initialize the circular buffer
 * 
 * @param buffer Pointer to the circular buffer
 */
void circular_buffer_init(circular_buffer_t *buffer);

/**
 * @brief Check if the buffer is full
 * 
 * @param buffer Pointer to the circular buffer
 * @return int 1 if full, 0 otherwise
 */
int circular_buffer_is_full(circular_buffer_t *buffer);

/**
 * @brief Check if the buffer is empty
 * 
 * @param buffer Pointer to the circular buffer
 * @return int 1 if empty, 0 otherwise
 */
int circular_buffer_is_empty(circular_buffer_t *buffer);

/**
 * @brief Insert an alarm into the circular buffer
 * Note: Caller must ensure buffer is not full before calling
 *
 * @param buffer Pointer to the circular buffer
 * @param alarm Pointer to the alarm to insert
 * @return int Index where the alarm was inserted
 */
int circular_buffer_insert(circular_buffer_t *buffer, alarm_t *alarm);

/**
 * @brief Remove an alarm from the circular buffer
 * Note: Caller must ensure buffer is not empty before calling
 *
 * @param buffer Pointer to the circular buffer
 * @param index Pointer to store the index where the alarm was removed from
 * @return alarm_t* Pointer to the removed alarm
 */
alarm_t *circular_buffer_remove(circular_buffer_t *buffer, int *index);

#endif /* CIRCULAR_BUFFER_H */

