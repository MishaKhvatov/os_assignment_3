
/**
 * @file console.c
 * @brief console.c provides an enhanced console interface that ensures user input remains visible during multithreaded application interactions by automatically rewriting user input when the console is updated.
 * 
 * Detailed description:
 * 
 * In multithreaded applications, when the console prompt is updated by one thread while the user is typing, the input is often overwritten or lost. This is particularly problematic when notifications or other outputs are printed to the console, causing the user's typed input to disappear.
 * 
 * `console.c` solves this issue by providing the `console_print()` function. This function ensures that the user's input is preserved and visible, even when the console prompt is refreshed. By automatically re-rendering both the prompt and the user’s input, it maintains a smooth and continuous user experience.
 * 
 * For example, without `console.c`, user input would be lost when the console prompt updates:
 * 
 * 1. User types: `hello world`
 *    ```
 *    > hel
 *    ```
 * 2. The console updates with a notification:
 *    ```
 *    [notification]
 *    >
 *    ```
 * With `console.c`, the user’s input is retained and re-displayed after the console update:
 * 
 * 1. User types: `hello world`
 *    ```
 *    > hel
 *    ```
 * 2. The console updates with a notification, but the user input remains visible:
 *    ```
 *    [notification]
 *    > hel
 *    ```
 * 
 * This functionality improves the user experience in applications with dynamic console outputs.
 * 
 * @author Misha Khvatov
 * @date 2025-03-5
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <termios.h>
#include "console.h"

#define MAX_INPUT 256
#define MAX_NOTIFICATIONS 10

char input_buffer[MAX_INPUT] = {0};
int input_pos = 0;
pthread_mutex_t lock;
struct termios original_termios; /* Original Settings for terminal */

void console_init(){
    /* Set the terminal to raw mode for character by character input */
    struct termios raw;
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(restore_terminal);

    pthread_mutex_init(&lock, NULL);
    
    raw = original_termios;
    raw.c_lflag &= ~(ECHO | ICANON);  /*  Disable echo & line buffering */
    raw.c_cc[VMIN] = 1;               /*  Read one character at a time */
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void restore_terminal(){
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

/* Thread-safe console printing function */
void console_print(const char *format, ...){
    pthread_mutex_lock(&lock);
    printf("\r\033[K");
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\nAlarm> %s", input_buffer);
    fflush(stdout);

    pthread_mutex_unlock(&lock);
}
/* Gets user input */
char* input(){
    char* ret_str;
    while(1){
        char ch;
        read(STDIN_FILENO, &ch,1);
        pthread_mutex_lock(&lock);

        if(ch == '\n'){
            input_buffer[input_pos] = '\0'; /* Null Terminate String */
            
            ret_str = malloc(strlen(input_buffer) +1);
            strcpy(ret_str, input_buffer);

            input_pos = 0;
            memset(input_buffer, 0 , MAX_INPUT);
            pthread_mutex_unlock(&lock);
            return ret_str;
        }
        /* handle backspace */
        else if(ch == 127){
            if(input_pos <= 0) {
                pthread_mutex_unlock(&lock);
                continue;
            }
            input_pos--;
            input_buffer[input_pos] = '\0';
            printf("\b \b");
            fflush(stdout);
        }
        else{
            if(input_pos >= MAX_INPUT - 1) {
                pthread_mutex_unlock(&lock);
                continue;
            }
            input_buffer[input_pos++] = ch;
            printf("%c",ch);
            fflush(stdout);
        }
        
        pthread_mutex_unlock(&lock);
    }
}


