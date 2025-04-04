#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H

#include <termios.h>
#include <pthread.h>

#define MAX_INPUT 256
#define MAX_NOTIFICATIONS 10

/**
 * @brief Initialize the console for raw input mode
 * 
 * Sets up the terminal to read input character by character,
 * disabling line buffering and echo. Registers an exit handler
 * to restore original terminal settings.
 */
void console_init(void);


/**
 * @brief Restore the original terminal settings
 * 
 * Reverts terminal settings to their state before console_init() was called.
 */
void restore_terminal(void);


/**
 * @brief Thread-safe console print function
 * 
 * Prints a message to the console while preserving current user input.
 * Uses mutex to ensure thread-safe output.
 * 
 * @param msg Message to be printed
 */

void console_print(const char *format,...);

/**
 * @brief Read user input from the console
 * 
 * Reads input character by character, supporting backspace 
 * and preventing buffer overflow. Returns a dynamically 
 * allocated string with the input.
 * 
 * @return Dynamically allocated string containing user input
 * @note Caller is responsible for freeing the returned string
 */
char* input(void);

#endif /*  CONSOLE_INPUT_H */


