#include "tree-map.h"
#include "object-cache.h"
#include "once.h"

static once_t init_control = ONCE_INIT;

/* Allocates internal_node's */
struct object_cache internal_node_cache;

/* Allocates tree_map's */
struct object_cache tree_map_cache;

/* Forward declaration */
struct _internal_node;

struct tree_map
{
    struct _internal_node * root;

    tree_map_compare_func comparator;
};

typedef struct _internal_node
{
    struct _internal_node * left;
    struct _internal_node * right;

    tree_map_key_t     key;
    tree_map_value_t   value;
} internal_node;

static unsigned int internal_node_height (internal_node * node);

static internal_node * internal_insert (
        struct tree_map * tree,
        internal_node * node,
        tree_map_key_t key,
        tree_map_value_t value,
        tree_map_value_t * prev_value
        );

static internal_node * internal_remove (
        struct tree_map * tree,
        internal_node * node,
        tree_map_key_t key,
        tree_map_value_t * prev_value
        );

static internal_node * internal_unlink_max (
        struct tree_map * tree,
        internal_node * node,
        internal_node ** max
        );

static void internal_free_node (
        struct tree_map * tree,
        internal_node * node
        );

/**
 * One-time initialization before any trees can be created.
 */
static void tree_map_static_init (void * param)
{
    object_cache_init(&internal_node_cache, sizeof(internal_node));
    object_cache_init(&tree_map_cache, sizeof(struct tree_map));
}

struct tree_map * tree_map_alloc (tree_map_compare_func comparator)
{
    struct tree_map * result;

    once(&init_control, tree_map_static_init, NULL);

    result = object_cache_alloc(&tree_map_cache);
    result->root = NULL;
    result->comparator = comparator;

    internal_node_height(result->root);

    return result;
}

void tree_map_free (struct tree_map * tree)
{
    internal_free_node(tree, tree->root);
    object_cache_free(&tree_map_cache, tree);
}

tree_map_value_t tree_map_insert (
        struct tree_map * tree,
        tree_map_key_t key,
        tree_map_value_t value
        )
{
    tree_map_value_t prev_value = NULL;

    tree->root = internal_insert(tree, tree->root, key, value, &prev_value);

    return prev_value;
}

tree_map_value_t tree_map_remove (
        struct tree_map * tree,
        tree_map_key_t key
        )
{
    tree_map_value_t prev_value = NULL;

    tree->root = internal_remove(tree, tree->root, key, &prev_value);

    return prev_value;
}

tree_map_value_t tree_map_lookup (
        struct tree_map * tree,
        tree_map_key_t key
        )
{
    internal_node * node;
    int compare_val;

    node = tree->root;
    compare_val = node ? tree->comparator(key, node->key) : 0;

    while (node && compare_val) {
        node = compare_val < 0 ? node->left : node->right;
        compare_val = node ? tree->comparator(key, node->key) : 0;
    }

    return node ? node->value : NULL;
}

static internal_node * internal_insert (
        struct tree_map * tree,
        internal_node * node,
        tree_map_key_t key,
        tree_map_value_t value,
        tree_map_value_t * prev_value
        )
{
    if (node) {
        int compare_val = tree->comparator(key, node->key);

        if (compare_val == 0) {
            /* Replacing what's already here */
            *prev_value = node->value;
            return node;
        }
        else if (compare_val > 0) {
            /* New key is greater than this node. */
            node->right = internal_insert(tree, node->right, key, value, prev_value);

            // XXX: rebalance
            return node;
        }
        else {
            /* New key is less than this node. */
            node->left = internal_insert(tree, node->left, key, value, prev_value);

            // XXX: rebalance
            return node;
        }
    }
    else {
        internal_node * new_node = object_cache_alloc(&internal_node_cache);

        new_node->left = NULL;
        new_node->right = NULL;
        new_node->key = key;
        new_node->value = value;
        *prev_value = NULL;

        return new_node;
    }
}

static internal_node * internal_remove (
        struct tree_map * tree,
        internal_node * node,
        tree_map_key_t key,
        tree_map_value_t * prev_value
        )
{
    internal_node * result;

    if (node) {
        int compare_val = tree->comparator(key, node->key);

        if (compare_val == 0) {
            /* Removing this node */
            *prev_value = node->value;

            if (!node->left && !node->right) {
                /* Both subtrees are empty */
                object_cache_free(&internal_node_cache, node);
                result = NULL;
            }
            else if (node->left && !node->right) {
                /* Only left subtree is nonempty */
                result = node->left;
                object_cache_free(&internal_node_cache, node);
            }
            else if (!node->left && node->right) {
                /* Only right subtree is nonempty */
                result = node->right;
                object_cache_free(&internal_node_cache, node);
            }
            else {
                /*
                Both subtrees are nonempty. Promote the maximum element
                of the left subtree to be the new root node.
                */
                node->left = internal_unlink_max(tree, node->left, &result);
                result->left = node->left;
                result->right = node->right;
                object_cache_free(&internal_node_cache, node);
            }
        }
        else if (compare_val > 0) {
            /* Key is greater than us. If a match exists, it's in right subtree. */
            node->right = internal_remove(tree, node->right, key, prev_value);

            // XXX: rebalance
            result = node;
        }
        else {
            /* Key is less than us. If a match exists, it's in left subtree. */
            node->left = internal_remove(tree, node->left, key, prev_value);

            // XXX: rebalance
            result = node;
        }
    }
    else {
        *prev_value = NULL;
        result = NULL;
    }

    return result;
}

/**
 * Walks @node and removes the maximum element from it. The resulting tree
 * minus the max element is returned. @node is not guaranteed to be valid
 * afterward; always assign the return value of this function to the pointer
 * which @node formerly occupied.
 */
static internal_node * internal_unlink_max (
        struct tree_map * tree,
        internal_node * node,
        internal_node ** max
        )
{
    if (node->right) {
        node->right = internal_unlink_max(tree, node->right, max);
        return node;
    }
    else if (node->left) {
        *max = node;
        return node->left;
    }
    else {
        *max = node;
        return NULL;
    }
}

static void internal_free_node (
        struct tree_map * tree,
        internal_node * node
        )
{
    if (node) {
        internal_free_node(tree, node->left);
        internal_free_node(tree, node->right);
        object_cache_free(&internal_node_cache, node);
    }
}

static unsigned int internal_node_height (internal_node * node)
{
    if (!node) {
        return 0;
    }
    else {
        unsigned int left_height = internal_node_height(node->left);
        unsigned int right_height = internal_node_height(node->right);

        return (left_height < right_height ? right_height : left_height) + 1;
    }
}
