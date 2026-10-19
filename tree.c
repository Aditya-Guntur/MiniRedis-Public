/*This the place where the whole program implementation will occur*/
#define _GNU_SOURCE  // For strdup
#include "tree.h"
#include "command_handler.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>  // For malloc, free
#include <assert.h>  // For assert
#include <strings.h> // For strcasecmp

// Forward declarations
void print_search_result(Leaf *result, const int8 *key);
void tree_cleanup(void);

Tree root = {
    .n = {
        .tag = (TagRoot | TagNode),
        .north = (Node *)&root,
        .west = NULL,
        .east = NULL,
        .path = "/"
    }
};

void zero(int8 *str, int16 size) {
    int8 *p;
    int16 n;
    for (n = 0, p = str; n < size; p++, n++) {
        *p = 0;
    }
}

Node *create_node(Node *parent, const int8 *path) {
    Node *n;
    int16 size;

    errno = NoError;
    assert(parent);
    
    size = sizeof(struct s_node);
    n = (Node *)malloc(size);
    if (!n) {
        errno = ENOMEM;
        return NULL;
    }
    
    zero((int8 *)n, size);
    n->tag = TagNode;
    n->north = parent;
    n->west = NULL;
    n->east = NULL;
    strncpy((char *)n->path, (char *)path, 255);
    n->path[255] = '\0';
    
    return n;
}

/**
 * Update the value of an existing leaf node
 * @param root The root node to start searching from
 * @param key The key of the leaf to update
 * @param new_value The new value to set
 * @param new_size The size of the new value
 * @return 0 on success, -1 on error
 */
int update_leaf(Node *root, const int8 *key, const int8 *new_value, int16 new_size) {
    Leaf *leaf;
    int8 *new_value_copy;
    
    // Validate input parameters
    if (!root || !key || !new_value || new_size <= 0) {
        errno = EINVAL;
        return -1;
    }
    
    // Find the leaf to update
    leaf = search_leaf(root, key);
    if (!leaf) {
        // search_leaf sets errno to ENOENT if the key is not found
        return -1;
    }
    
    // Allocate memory for the new value
    new_value_copy = (int8 *)malloc(new_size);
    if (!new_value_copy) {
        errno = ENOMEM;
        return -1;
    }
    
    // Copy the new value
    zero(new_value_copy, new_size);
    strncpy((char *)new_value_copy, (const char *)new_value, new_size - 1);
    new_value_copy[new_size - 1] = '\0';
    
    // Free the old value and update the leaf
    if (leaf->value) {
        free(leaf->value);
    }
    
    leaf->value = new_value_copy;
    leaf->size = new_size;
    
    return 0;
}

/**
 * Delete a leaf node with the given key from the tree
 * @param root The root node to start searching from
 * @param key The key of the leaf to delete
 * @return 0 on success, -1 on error
 */
int delete_leaf(Node *root, const int8 *key) {
    // prev/current used to be declared Leaf* and walked via
    // current_leaf = current_leaf->east, which assumed every sibling
    // under root was a Leaf. The moment a subdirectory was mixed in
    // (anywhere in the chain, not just adjacent to the target key),
    // this would misread a Node's bytes as a Leaf's.
    Tree *prev = NULL;
    Tree *current = root->west;

    if (!root || !key) {
        errno = EINVAL;
        return -1;
    }

    while (current != NULL) {
        if (!(current->l.tag & TagLeaf)) {
            prev = current;
            current = current->n.east;
            continue;
        }

        if (strcmp((const char *)current->l.key, (const char *)key) != 0) {
            prev = current;
            current = current->l.east;
            continue;
        }

        // Found it - unlink from whatever precedes it (or root, if it
        // was the first child).
        Tree *next = current->l.east;
        if (prev) {
            if (prev->n.tag & (TagNode | TagRoot)) prev->n.east = next;
            else prev->l.east = next;
        } else {
            root->west = next;
        }

        if (current->l.value) free(current->l.value);
        if (current->l.key) free(current->l.key);
        free(current);

        errno = NoError;
        return 0;
    }

    errno = ENOENT;
    return -1;
}

// Forward declarations
void print_search_result(Leaf *result, const int8 *key);


// Walks parent's child chain to the last sibling, whatever mix of
// subdirectories and files it holds. This used to cast parent->east
// straight to Leaf* and walk it as a leaf-only chain (same assumption
// delete_leaf and search_leaf made), which misread a Node's bytes as
// a Leaf's whenever a subdirectory came before a file under the same
// parent.
Tree *find_last_linear(Node *parent) {
    Tree *t;

    if (!parent) {
        errno = EINVAL;
        return NULL;
    }

    if (!parent->west) {
        errno = NoError;
        return NULL;
    }

    t = parent->west;
    while (1) {
        Tree *next = (t->n.tag & (TagNode | TagRoot)) ? t->n.east : t->l.east;
        if (!next) break;
        t = next;
    }

    return t;
}

Leaf *create_leaf(Node *parent, const int8 *key, const int8 *value, int16 count) {
    Tree *last_sibling;
    Leaf *new_leaf;

    if (!parent || !key || !value || count <= 0) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate and initialize new leaf first
    new_leaf = (Leaf *)malloc(sizeof(struct s_leaf));
    if (!new_leaf) {
        errno = ENOMEM;
        return NULL;
    }

    // Initialize the new leaf
    zero((int8 *)new_leaf, sizeof(struct s_leaf));
    new_leaf->tag = TagLeaf;

    // Find the last sibling (subdirectory or file) under this parent
    last_sibling = find_last_linear(parent);
    if (last_sibling == NULL && errno != NoError) {
        free(new_leaf);
        return NULL;
    }

    // Link the new leaf to the parent or the last sibling, whichever
    // type that sibling actually is.
    if (!last_sibling) {
        // First child for this parent
        parent->west = (Tree *)new_leaf;
        new_leaf->west = (Tree *)parent;
    } else if (last_sibling->n.tag & (TagNode | TagRoot)) {
        last_sibling->n.east = (Tree *)new_leaf;
        new_leaf->west = last_sibling;
    } else {
        last_sibling->l.east = (Tree *)new_leaf;
        new_leaf->west = last_sibling;
    }
    
    // Allocate and copy the key
    new_leaf->key = (int8 *)malloc(strlen((char *)key) + 1);
    if (!new_leaf->key) {
        free(new_leaf);
        errno = ENOMEM;
        return NULL;
    }
    strcpy((char *)new_leaf->key, (char *)key);
    
    // Allocate and copy the value
    new_leaf->value = (int8 *)malloc(count);
    if (!new_leaf->value) {
        free(new_leaf);
        errno = ENOMEM;
        return NULL;
    }
    
    zero(new_leaf->value, count);
    strncpy((char *)new_leaf->value, (char *)value, count - 1);
    new_leaf->value[count - 1] = '\0';
    new_leaf->size = count;
    
    return new_leaf;
}


/**
 * Search for a leaf node with the given key in the tree
 * @param root The root node to start searching from
 * @param key The key to search for
 * @return Pointer to the Leaf if found, NULL otherwise
 */
Leaf *search_leaf(const Node *root, const int8 *key) {
    Tree *current;

    // Validate input parameters
    if (!root || !key) {
        errno = EINVAL;  // Invalid argument
        return NULL;
    }

    // Walk root's children, skipping subdirectories. This used to cast
    // root->east straight to Leaf* and walk it as a leaf-only chain,
    // which misread a Node's bytes as a Leaf's (and silently missed
    // any real leaf sitting past a subdirectory in the same chain)
    // whenever a parent had both kinds of children.
    current = root->west;
    while (current != NULL) {
        if (current->l.tag & TagLeaf) {
            if (strcmp((const char *)current->l.key, (const char *)key) == 0) {
                errno = NoError;
                return &current->l;
            }
            current = current->l.east;
        } else {
            current = current->n.east;
        }
    }

    // If we get here, the key was not found
    errno = ENOENT;  // No such entry
    return NULL;
}

/**
 * Search for a node with the given path in the tree
 * @param root The root node to start searching from
 * @param path The path to search for (e.g., "/path/to/node")
 * @return Pointer to the Node if found, NULL otherwise
 */
Node *search_node(const Node *root, const int8 *path) {
    // For now, we'll implement a simple search that only looks at the direct children
    // This can be enhanced to handle full path traversal
    
    // Validate input parameters
    if (!root || !path) {
        errno = EINVAL;
        return NULL;
    }
    
    // Check if the path is the root
    if (strcmp((const char *)root->path, (const char *)path) == 0) {
        errno = NoError;
        return (Node *)root;
    }
    
    // For now, we'll just return NULL as we need to implement proper path traversal
    errno = ENOENT;
    return NULL;
}

// Forward declaration for print_search_result
void print_search_result(Leaf *result, const int8 *key);

// Recursive function to free all nodes and leaves
// Recursing into "children" and continuing to "the next sibling" used
// to be the same operation here (both just followed ->east), which
// happened to free everything reachable at all only because the two
// relationships were never cleanly separated in the first place - see
// tree.h. Now that west is first-child and east is next-sibling, both
// have to be walked explicitly.
static void free_tree(Tree *tree) {
    if (!tree) return;

    if (tree->n.tag & (TagNode | TagRoot)) {
        if (tree->n.west) {
            free_tree(tree->n.west); // this node's children
        }
        if (tree->n.east) {
            free_tree(tree->n.east); // its next sibling
        }
        // Free the node itself if it's not the root
        if (!(tree->n.tag & TagRoot)) {
            free(tree);
        }
    } else if (tree->l.tag & TagLeaf) {
        if (tree->l.east) {
            free_tree(tree->l.east); // its next sibling
        }
        if (tree->l.key) {
            free(tree->l.key);
        }
        if (tree->l.value) {
            free(tree->l.value);
        }
        free(tree);
    }
}

// Function to free all resources
void tree_cleanup() {
    printf("Cleaning up memory...\n");

    if (root.n.west) {
        free_tree(root.n.west);
        root.n.west = NULL;
    }

    printf("Memory cleanup completed.\n");
}

// Helper function to print search results
void print_search_result(Leaf *result, const int8 *key) {
    if (result) {
        printf("Found key '%s' with value: %.*s\n", 
               key, result->size, result->value);
    } else {
        if (errno == ENOENT) {
            printf("Key '%s' not found\n", key);
        } else {
            printf("Error searching for key '%s': %s\n", key, strerror(errno));
        }
    }
}

int main(int argc, char *argv[]) {
    // Initialize root node if not already initialized
    if (!(root.n.tag & TagRoot)) {
        root.n.tag = TagRoot | TagNode;
        root.n.north = (Node *)&root;
        root.n.west = NULL;
        root.n.east = NULL;
        strcpy((char *)root.n.path, "/");
    }

    // Check if we should run in test mode or interactive mode
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        // Run tests if --test flag is provided
        printf("\n=== Running Tests ===\n");
        
        // Test 1: Create and search for a leaf
        printf("\nTest 1: Creating and searching for a leaf...\n");
        int8 key1[128] = "test_key";
        int8 value1[256] = "test_value";
        Leaf *l1 = create_leaf(&root.n, key1, value1, strlen((char *)value1) + 1);
        if (!l1) {
            printf("Failed to create leaf: %s\n", strerror(errno));
            return 1;
        }
        printf("Created leaf with key '%s' and value '%s'\n", key1, l1->value);
        
        // Test 2: Search for a non-existent key
        printf("\nTest 2: Searching for a non-existent key...\n");
        int8 non_existent_key[128] = "non_existent";
        Leaf *search_result = search_leaf(&root.n, non_existent_key);
        print_search_result(search_result, non_existent_key);

        // Test 3: Test update functionality
        printf("\nTest 3: Testing update functionality...\n");
        int8 updated_value[256] = "updated_test_value";
        printf("Updating value for key 'test_key'...\n");
        if (update_leaf(&root.n, key1, updated_value, strlen((char *)updated_value) + 1) == 0) {
            printf("Successfully updated the value. New value: %s\n", updated_value);
            
            // Verify the update
            printf("Verifying update...\n");
            search_result = search_leaf(&root.n, key1);
            print_search_result(search_result, key1);
        } else {
            printf("Failed to update leaf: %s\n", strerror(errno));
        }

        // Test 4: Test delete functionality
        printf("\nTest 4: Testing delete functionality...\n");
        printf("Deleting leaf with key 'test_key'...\n");
        if (delete_leaf(&root.n, key1) == 0) {
            printf("Successfully deleted leaf with key 'test_key'\n");
            
            // Verify the leaf was deleted
            search_result = search_leaf(&root.n, key1);
            printf("Verifying deletion...\n");
            print_search_result(search_result, key1);
        } else {
            printf("Failed to delete leaf: %s\n", strerror(errno));
        }

        printf("\n=== All tests completed ===\n");
    } else {
        // Start interactive REPL
        start_repl(&root.n);
    }

    // Clean up resources
    tree_cleanup();
    
    return 0;
}
