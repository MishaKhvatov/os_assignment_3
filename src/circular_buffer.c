/**
 * @file circular_buffer.c
 * @brief Implementation of circular buffer for alarm system
 */

#include "circular_buffer.h"

/**
 * @brief Initialize the circular buffer
 * 
 * @param buffer Pointer to the circular buffer
 */
void circular_buffer_init(circular_buffer_t *buffer) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
}

/**
 * @brief Check if the buffer is full
 * 
 * @param buffer Pointer to the circular buffer
 * @return int 1 if full, 0 otherwise
 */
int circular_buffer_is_full(circular_buffer_t *buffer) {
    return buffer->count == CIRCULAR_BUFFER_SIZE;
}

/**
 * @brief Check if the buffer is empty
 * 
 * @param buffer Pointer to the circular buffer
 * @return int 1 if empty, 0 otherwise
 */
int circular_buffer_is_empty(circular_buffer_t *buffer) {
    return buffer->count == 0;
}

/**
 * @brief Insert an alarm into the circular buffer
 * Note: Caller must ensure buffer is not full before calling
 *
 * @param buffer Pointer to the circular buffer
 * @param alarm Pointer to the alarm to insert
 * @return int Index where the alarm was inserted
 */
int circular_buffer_insert(circular_buffer_t *buffer, alarm_t *alarm) {
    int insert_index;
    pthread_mutex_lock(&buffer->mutex);

    /* Wait until buffer is not full */
    while (buffer->count == CIRCULAR_BUFFER_SIZE) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    insert_index = buffer->head;
    buffer->alarms[buffer->head] = alarm;
    buffer->head = (buffer->head + 1) % CIRCULAR_BUFFER_SIZE;
    buffer->count++;

    /* Signal that buffer is not empty */
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    return insert_index;
}


/**
 * @brief Remove an alarm from the circular buffer
 * Note: Caller must ensure buffer is not empty before calling
 *
 * @param buffer Pointer to the circular buffer
 * @param index Pointer to store the index where the alarm was removed from
 * @return alarm_t* Pointer to the removed alarm
 */
alarm_t *circular_buffer_remove(circular_buffer_t *buffer, int *index) {
    alarm_t *alarm;
    pthread_mutex_lock(&buffer->mutex);

    /* Wait until buffer is not empty */
    while (buffer->count == 0) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    *index = buffer->tail;
    alarm = buffer->alarms[buffer->tail];
    buffer->tail = (buffer->tail + 1) % CIRCULAR_BUFFER_SIZE;
    buffer->count--;

    /* Signal that buffer is not full */
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);

    return alarm;
}
