#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

// Common type definitions
typedef int8_t int8;
typedef int16_t int16;

typedef enum {
    NoError = 0,
    TagRoot = 1 << 0,
    TagNode = 1 << 1,
    TagLeaf = 1 << 2
} Tag;

typedef struct s_node Node;
typedef struct s_leaf Leaf;

typedef union Tree {
    Node n;
    Leaf l;
} Tree;

struct s_node {
    Tag tag;
    Node *north;
    Node *west;
    Tree *east;
    int8 path[256];
};

struct s_leaf {
    Tag tag;
    Node *west;
    Tree *east;
    int8 *key;
    int8 *value;
    int16 size;
};

// Function declarations from tree.h
Node *create_node(Node *parent, int8 *path);
Leaf *create_leaf(Node *parent, int8 *key, int8 *value, int16 count);
Leaf *search_leaf(Node *root, const int8 *key);
Node *search_node(Node *root, const int8 *path);
int update_leaf(Node *root, const int8 *key, const int8 *new_value, int16 new_size);
int delete_leaf(Node *root, const int8 *key);
void tree_cleanup(void);

// Global root node
extern Tree root;

#endif // COMMON_H
