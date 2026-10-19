#ifndef TREE_H
#define TREE_H

#include <stdint.h>

// Type definitions
typedef int8_t int8;
typedef int16_t int16;
typedef uint32_t uint32;

typedef enum {
    NoError = 0,
    TagRoot = 1 << 0,
    TagNode = 1 << 1,
    TagLeaf = 1 << 2
} Tag;

// Forward declarations
typedef struct s_node Node;
typedef struct s_leaf Leaf;
typedef union u_tree Tree;

// Structure definitions
struct s_node {
    Tag tag;
    Node *north;
    Node *west;
    Tree *east;
    int8 path[256];
};

struct s_leaf {
    Tag tag;
    Tree *west;
    Leaf *east;
    int8 *key;
    int8 *value;
    int16 size;
};

union u_tree {
    Node n;
    Leaf l;
};

// Global root node
extern Tree root;

// Function declarations
Node *create_node(Node *parent, const int8 *path);
Leaf *create_leaf(Node *parent, const int8 *key, const int8 *value, int16 count);
Leaf *search_leaf(const Node *root, const int8 *key);
Node *search_node(const Node *root, const int8 *path);
int update_leaf(Node *root, const int8 *key, const int8 *new_value, int16 new_size);
int delete_leaf(Node *root, const int8 *key);
void tree_cleanup(void);

// Helper macros
#define find_last(x) find_last_linear(x)
#define reterr(x) \
    do { \
        errno = (x); \
        return NULL; \
    } while(0)

#endif // TREE_H