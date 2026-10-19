#define _GNU_SOURCE  // For strdup and strcasecmp
#include "command_handler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <strings.h>  // For strcasecmp
#include <ctype.h>    // For isspace, toupper, tolower
#include <unistd.h>   // For getline
#include <stdbool.h>  // For bool type

// Define ENOTSUP if not already defined
#ifndef ENOTSUP
#define ENOTSUP ENOSYS  // Use ENOSYS (Function not implemented) as a fallback
#endif

// Include the header to get the Command structure and function declarations
#include "command_handler.h"

// Windows compatibility for strtok_r
#ifdef _WIN32
#include <string.h>
char* strtok_r(char* str, const char* delim, char** saveptr) {
    if (str != NULL) {
        *saveptr = str;
    } else if (*saveptr == NULL) {
        return NULL;
    }
    
    // Skip leading delimiters
    char* token = *saveptr;
    while (*token && strchr(delim, *token)) {
        token++;
    }
    
    if (!*token) {
        *saveptr = NULL;
        return NULL;
    }
    
    // Find the end of the token
    char* end = token;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = NULL;
    }
    
    return token;
}
#endif

// Available commands
static const Command commands[] = {
    {"SET", (command_handler_t)handle_set, "SET <key> <value> - Set a key-value pair"},
    {"GET", (command_handler_t)handle_get, "GET <key> - Get the value for a key"},
    {"DEL", (command_handler_t)handle_del, "DEL <key> - Delete a key-value pair"},
    {"EXISTS", (command_handler_t)handle_exists, "EXISTS <key> - Check if a key exists"},
    {"MKDIR", (command_handler_t)handle_mkdir, "MKDIR <path> - Create a new directory"},
    {"CD", (command_handler_t)handle_cd, "CD <path> - Change current directory"},
    {"LS", (command_handler_t)handle_ls, "LS - List contents of current directory"},
    {"PWD", (command_handler_t)handle_pwd, "PWD - Print working directory"},
    {"HELP", (command_handler_t)handle_help, "HELP - Show this help message"},
    {NULL, NULL, NULL} // Sentinel
};

// Forward declarations of helper functions
char *trim_whitespace(char *str);
char **split_path(const char *path, int *count);
void free_path_components(char **components, int count);

// Navigation command handlers
void handle_cd(void **root_ptr, const char *path) {
    if (!root_ptr || !*root_ptr) {
        printf("Error: Invalid root pointer\n");
        return;
    }
    
    // Cast the void** to Node** for safe dereferencing
    Node **node_ptr = (Node **)root_ptr;
    Node *root = *node_ptr;
    
    // Handle root directory
    if (!path || !*path || strcmp(path, "/") == 0) {
        while (root->north) {
            root = root->north;
        }
        *node_ptr = root;
        return;
    }
    
    // Handle parent directory
    if (strcmp(path, "..") == 0) {
        if (root->north) {
            *node_ptr = root->north;
        }
        return;
    }
    
    // Handle current directory
    if (strcmp(path, ".") == 0) {
        return;
    }
    
    // Handle absolute paths
    Node *current = *root_ptr;
    if (path[0] == '/') {
        // Go to root
        while (current->north) current = current->north;
        path++; // Skip the leading '/'
        if (!*path) {
            *root_ptr = current;
            return;
        }
    }
    
    // Split path into components
    int count = 0;
    char **components = split_path(path, &count);
    if (!components) {
        printf("Error: Invalid path\n");
        return;
    }
    
    // Traverse the path
    for (int i = 0; i < count; i++) {
        if (strcmp(components[i], "..") == 0) {
            if (current->north) {
                current = current->north;
            }
        } else if (strcmp(components[i], ".") != 0) {
            // Look for the child node with this name
            Tree *child = current->east;
            Node *found = NULL;
            
            while (child) {
                if (child->n.tag & TagNode) {
                    if (strcmp((char *)child->n.path, components[i]) == 0) {
                        found = &child->n;
                        break;
                    }
                }
                child = (child->n.tag & TagNode) ? child->n.east : (Tree *)((Leaf *)child)->east;
            }
            
            if (!found) {
                printf("Error: No such directory: %s\n", components[i]);
                free_path_components(components, count);
                return;
            }
            
            current = found;
        }
    }
    
    *root_ptr = current;
    free_path_components(components, count);
}

// Create a new directory
void handle_mkdir(void *root_ptr, const char *path) {
    Node *root = (Node *)root_ptr;
    if (!path || !*path) {
        printf("Error: Missing directory name. Usage: MKDIR <path>\n");
        return;
    }
    
    // Handle absolute paths
    if (path[0] == '/') {
        // Go to root
        while (root->north) root = root->north;
        path++; // Skip the leading '/'
        if (!*path) {
            printf("Error: Cannot create root directory\n");
            return;
        }
    }
    
    // Split path into components
    int count = 0;
    char **components = split_path(path, &count);
    if (!components || count == 0) {
        printf("Error: Invalid path\n");
        return;
    }
    
    // Navigate to the parent directory
    Node *current = root;
    for (int i = 0; i < count - 1; i++) {
        Tree *child = current->east;
        Node *found = NULL;
        
        while (child) {
            if (child->n.tag & TagNode) {
                if (strcmp((char *)child->n.path, components[i]) == 0) {
                    found = &child->n;
                    break;
                }
            }
            child = (child->n.tag & TagNode) ? child->n.east : (Tree *)((Leaf *)child)->east;
        }
        
        if (!found) {
            printf("Error: No such directory: %s\n", components[i]);
            free_path_components(components, count);
            return;
        }
        
        current = found;
    }
    
    // Create the new directory
    Node *new_node = create_node(current, (int8 *)components[count-1]);
    if (!new_node) {
        printf("Error: Failed to create directory: %s\n", strerror(errno));
        free_path_components(components, count);
        return;
    }
    
    // Link the new node into the tree
    if (!current->east) {
        // First child of this node
        current->east = (Tree *)new_node;
    } else {
        // Add to the end of the sibling list
        Tree *sibling = current->east;
        Tree *last_sibling = NULL;
        
        // Find the last sibling in the list
        while (1) {
            if (sibling->n.tag & TagNode) {
                // It's a node
                if (!sibling->n.east) {
                    last_sibling = sibling;
                    break;
                }
                sibling = sibling->n.east;
            } else if (sibling->l.tag & TagLeaf) {
                // It's a leaf
                if (!((Leaf *)sibling)->east) {
                    last_sibling = sibling;
                    break;
                }
                sibling = (Tree *)((Leaf *)sibling)->east;
            } else {
                // Unknown type, should not happen
                printf("Error: Unknown node type in sibling list\n");
                free_path_components(components, count);
                free(new_node);
                return;
            }
        }
        
        // Link the new node to the last sibling
        if (last_sibling) {
            if (last_sibling->n.tag & TagNode) {
                // Last sibling is a node
                Node *last_node = (Node *)last_sibling;
                last_node->east = (Tree *)new_node;
                // The west pointer should be set by create_node, so we don't need to set it here
            } else if (last_sibling->l.tag & TagLeaf) {
                // Last sibling is a leaf - we can't have a leaf as the last sibling when adding a new node
                // This should not happen as we should only be adding nodes to the node list
                printf("Error: Cannot add a directory after a leaf node\n");
                free_path_components(components, count);
                free(new_node);
                return;
            }
        }
    }
    
    printf("OK\n");
    
    free_path_components(components, count);
}

void handle_ls(const void *root_ptr) {
    const Node *root = (const Node *)root_ptr;
    if (!root) {
        printf("Error: Invalid directory\n");
        return;
    }
    
    Tree *current = root->east;
    int dir_count = 0;
    int file_count = 0;
    
    // First pass: count directories and files
    Tree *iter = current;
    while (iter) {
        if (iter->n.tag & (TagNode | TagRoot)) {
            dir_count++;
        } else if (iter->l.tag & TagLeaf) {
            file_count++;
        }
        iter = (iter->n.tag & (TagNode | TagRoot)) ? 
               iter->n.east : 
               (Tree *)((Leaf *)iter)->east;
    }
    
    // Print directory contents
    if (dir_count > 0) {
        printf("\x1B[1;34mDirectories (%d):\x1B[0m\n", dir_count);
        iter = current;
        while (iter) {
            if (iter->n.tag & (TagNode | TagRoot)) {
                printf("  \x1B[1;34m%-20s\x1B[0m  %-8s\n", 
                      iter->n.path, "<DIR>");
            }
            iter = (iter->n.tag & (TagNode | TagRoot)) ? 
                   iter->n.east : 
                   (Tree *)((Leaf *)iter)->east;
        }
    }
    
    // Print files
    if (file_count > 0) {
        if (dir_count > 0) printf("\n");
        printf("\x1B[1;32mFiles (%d):\x1B[0m\n", file_count);
        iter = current;
        while (iter) {
            if (iter->l.tag & TagLeaf) {
                printf("  \x1B[1;32m%-20s\x1B[0m  %-8zu bytes\n", 
                      iter->l.key, iter->l.size);
            }
            iter = (iter->n.tag & (TagNode | TagRoot)) ? 
                   iter->n.east : 
                   (Tree *)((Leaf *)iter)->east;
        }
    }
    
    if (dir_count == 0 && file_count == 0) {
        printf("Empty directory\n");
    } else {
        printf("<empty>\n");
    }
}

void handle_pwd(const void *root_ptr) {
    const Node *root = (const Node *)root_ptr;
    if (!root) {
        printf("/\n");
        return;
    }
    
    // In a full implementation, we would traverse up the tree to build the full path
    // For now, we'll just show the current node's path
    if (root->path[0]) {
        printf("/%s\n", root->path);
    } else {
        printf("/\n");
    }
}

// Helper function to split a path into components
char **split_path(const char *path, int *count) {
    if (!path || !count) return NULL;
    
    // Count the number of path components
    *count = 0;
    const char *p = path;
    while (*p) {
        if (*p == '/') (*count)++;
        p++;
    }
    if (path[strlen(path)-1] != '/') (*count)++;
    
    // Allocate array for path components
    char **components = malloc(*count * sizeof(char *));
    if (!components) return NULL;
    
    // Make a copy of the path for tokenization
    char *path_copy = strdup(path);
    if (!path_copy) {
        free(components);
        return NULL;
    }
    
    // Split the path into components
    int i = 0;
    char *token = strtok(path_copy, "/");
    while (token && i < *count) {
        components[i++] = strdup(token);
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    return components;
}

// Helper function to free path components
void free_path_components(char **components, int count) {
    if (!components) return;
    
    for (int i = 0; i < count; i++) {
        if (components[i]) free(components[i]);
    }
    
    free(components);
}

// Helper function to trim whitespace from the beginning and end of a string
char *trim_whitespace(char *str) {
    if (!str || !*str) {
        return str;  // Return as is for NULL or empty string
    }
    
    char *start = str;
    char *end = str + strlen(str) - 1;
    
    // Trim leading whitespace
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    // If the string was all whitespace
    if (start > end) {
        *str = '\0';
        return str;
    }
    
    // Trim trailing whitespace
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    
    // Null terminate the trimmed string
    *(end + 1) = '\0';
    
    return start;
}

// Command handlers implementation
void handle_set(void *root_ptr, const char *args) {
    Node *root = (Node *)root_ptr;
    if (!args || !*args) {
        printf("Error: Missing key and value. Usage: SET <key> <value>\n");
        return;
    }
    
    // Make a copy of args since we need to modify it
    char *args_copy = strdup(args);  // strdup is declared in string.h with _GNU_SOURCE
    if (!args_copy) {
        printf("Error: Out of memory\n");
        return;
    }
    
    // Find the first space to separate key and value
    char *space = strchr(args_copy, ' ');
    if (!space) {
        printf("Error: Missing value. Usage: SET <key> <value>\n");
        free(args_copy);
        return;
    }
    
    // Split key and value
    *space = '\0';
    char *key = args_copy;
    char *value = space + 1;
    
    // Create or update the leaf
    Leaf *leaf = search_leaf(root, (int8 *)key);
    if (leaf) {
        // Update existing leaf
        if (update_leaf(root, (int8 *)key, (int8 *)value, strlen(value) + 1) == 0) {
            printf("OK\n");
        } else {
            printf("Error updating key '%s': %s\n", key, strerror(errno));
        }
    } else {
        // Create new leaf
        leaf = create_leaf(root, (int8 *)key, (int8 *)value, strlen(value) + 1);
        if (leaf) {
            printf("OK\n");
        } else {
            printf("Error creating key '%s': %s\n", key, strerror(errno));
        }
    }
    
    free(args_copy);
}

void handle_get(const void *root_ptr, const char *args) {
    const Node *root = (const Node *)root_ptr;
    if (!args || !*args) {
        printf("Error: Missing key. Usage: GET <key>\n");
        return;
    }
    
    // Find the leaf
    Leaf *leaf = search_leaf(root, (int8 *)args);
    if (leaf) {
        printf("\"%s\"\n", leaf->value);
    } else {
        printf("(nil)\n");
    }
}

void handle_del(void *root_ptr, const char *args) {
    Node *root = (Node *)root_ptr;
    if (!args || !*args) {
        printf("Error: Missing key. Usage: DEL <key>\n");
        return;
    }
    
    if (delete_leaf(root, (int8 *)args) == 0) {
        printf("1\n"); // Return 1 for successful deletion
    } else {
        if (errno == ENOENT) {
            printf("0\n"); // Key didn't exist
        } else {
            printf("Error deleting key '%s': %s\n", args, strerror(errno));
        }
    }
}

void handle_exists(const void *root_ptr, const char *args) {
    const Node *root = (const Node *)root_ptr;
    if (!args || !*args) {
        printf("Error: Missing key. Usage: EXISTS <key>\n");
        return;
    }
    
    Leaf *leaf = search_leaf(root, (int8 *)args);
    printf("%d\n", leaf ? 1 : 0);
}

void handle_help(void *root_ptr, const char *args) {
    (void)root_ptr; // Unused parameter
    (void)args;  // Unused parameter
    
    printf("Available commands:\n");
    for (const Command *cmd = commands; cmd->name; cmd++) {
        printf("  %s\n", cmd->usage);
    }
}

void process_command(void **root_ptr, const char *input) {
    if (!input || !*input) {
        return; // Empty input
    }
    
    // Make a copy of the input for tokenization
    char input_copy[MAX_INPUT_LENGTH];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';
    
    // Trim whitespace
    trim_whitespace(input_copy);
    
    // Get the command and arguments
    char *command = input_copy;
    char *args = strchr(input_copy, ' ');
    
    // If there are arguments, split the string
    if (args) {
        *args = '\0';
        args++;
        trim_whitespace(args);
    }
    
    // Convert void** to Node** for CD command
    Node **node_ptr = (Node **)root_ptr;
    
    // Handle CD command specially since it needs the double pointer
    if (strcasecmp(command, "CD") == 0) {
        handle_cd(root_ptr, args ? args : "");
        return;
    }
    
    // Handle LS command specially since it's const
    if (strcasecmp(command, "LS") == 0) {
        handle_ls(*node_ptr);
        return;
    }
    
    // Handle PWD command specially since it's const
    if (strcasecmp(command, "PWD") == 0) {
        handle_pwd(*node_ptr);
        return;
    }
    
    // Find and execute the command
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcasecmp(command, commands[i].name) == 0) {
            commands[i].handler(*node_ptr, args ? args : "");
            return;
        }
    }
    
    // If we get here, the command wasn't found
    printf("Unknown command: %s\n", command);
    handle_help(*node_ptr, NULL);
}

// Start the REPL (Read-Eval-Print Loop)
void start_repl(Node *root) {
    char input[MAX_INPUT_LENGTH];
    
    printf("Database Server (Type 'HELP' for commands, 'EXIT' to quit)\n");
    
    // Create a pointer to the root for the REPL
    Node *current = root;
    
    while (1) {
        printf("db> ");
        fflush(stdout);
        
        // Read input
        if (!fgets(input, sizeof(input), stdin)) {
            printf("\n");
            break; // Exit on EOF (Ctrl+D)
        }
        
        // Remove newline
        input[strcspn(input, "\r\n")] = '\0';
        
        // Create a void pointer to the current node pointer for process_command
        void *current_void = (void *)current;
        
        // Process the command with the current directory
        process_command(&current_void, input);
        
        // Update the current pointer from the potentially modified void pointer
        current = (Node *)current_void;
        
        // Update the root pointer in case it changed (e.g., CD command)
        if (current->north == NULL) { // If we're at the root
            root = current;
        }
    }
}
