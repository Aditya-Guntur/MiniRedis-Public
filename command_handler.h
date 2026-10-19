#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "tree.h"
#include <stdint.h>

// Maximum length for command input
#define MAX_INPUT_LENGTH 1024

// Forward declaration of Node
typedef struct s_node Node;

// Command handler function type
typedef void (*command_handler_t)(void *root, const char *args);

// Command structure
typedef struct {
    const char *name;
    command_handler_t handler;
    const char *usage;
} Command;

// Command handlers
void handle_set(void *root_ptr, const char *args);
void handle_get(const void *root_ptr, const char *args);
void handle_del(void *root_ptr, const char *args);
void handle_exists(const void *root_ptr, const char *args);
void handle_help(void *root_ptr, const char *args);

// Navigation command handlers
void handle_cd(void **root_ptr, const char *path);
void handle_mkdir(void *root_ptr, const char *path);
void handle_ls(const void *root_ptr);
void handle_pwd(const void *root_ptr);

// Main command processing function
void process_command(void **root_ptr, const char *input);

// Start the REPL (Read-Eval-Print Loop)
void start_repl(Node *root);

// Helper functions
char *trim_whitespace(char *str);

#endif // COMMAND_HANDLER_H
