#include "assert.h"
#include "object-cache.h"
#include "once.h"
#include "tree-map.h"

#define max(_a, _b)     \
    ({                  \
    typeof(_a) a = _a;  \
    typeof(_b) b = _b;  \
    a > b ? a : b;      \
    })

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
    unsigned int size;

    tree_map_compare_func comparator;
};

typedef struct _internal_node
{
    struct _internal_node * left;
    struct _internal_node * right;
    int                     height;

    tree_map_key_t     key;
    tree_map_value_t   value;
} internal_node;

/* Needs to be signed integer to avoid wraparound when comparing results */
static int height (
        internal_node * node
        );

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

static internal_node * internal_rebalance (
        internal_node * node
        );

static internal_node * rotate_with_left_child (
        internal_node * k2
        );

static internal_node * double_with_left_child (
        internal_node * k3
        );

static internal_node * rotate_with_right_child (
        internal_node * k1
        );

static internal_node * double_with_right_child (
        internal_node * k1
        );

typedef void (*foreach_func) (
        internal_node * node
        );

static void internal_foreach (
        internal_node * node,
        foreach_func func
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
    result->size = 0;

    return result;
}

void tree_map_free (struct tree_map * tree)
{
    internal_free_node(tree, tree->root);
    object_cache_free(&tree_map_cache, tree);
}

static void check_subtree_balance (
        internal_node * node
        )
{
    int balance = height(node->left) - height(node->right);

    assert(balance >= -1 && balance <= 1);
}

tree_map_value_t tree_map_insert (
        struct tree_map * tree,
        tree_map_key_t key,
        tree_map_value_t value
        )
{
    tree_map_value_t prev_value = NULL;

    tree->root = internal_insert(tree, tree->root, key, value, &prev_value);
    internal_foreach(tree->root, check_subtree_balance);

    if (!prev_value) {
        tree->size++;
    }

    return prev_value;
}

tree_map_value_t tree_map_remove (
        struct tree_map * tree,
        tree_map_key_t key
        )
{
    tree_map_value_t prev_value = NULL;

    tree->root = internal_remove(tree, tree->root, key, &prev_value);

    if (prev_value) {
        tree->size--;
    }

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
            node = internal_rebalance(node);
            internal_foreach(node->right, check_subtree_balance);

            return node;
        }
        else {
            /* New key is less than this node. */
            node->left = internal_insert(tree, node->left, key, value, prev_value);
            node = internal_rebalance(node);
            internal_foreach(node->left, check_subtree_balance);
            return node;
        }
    }
    else {
        internal_node * new_node = object_cache_alloc(&internal_node_cache);

        new_node->left = NULL;
        new_node->right = NULL;
        new_node->height = 0;
        new_node->key = key;
        new_node->value = value;
        *prev_value = NULL;

        return new_node;
    }
}

unsigned int tree_map_size (
        struct tree_map * tree
        )
{
    return tree->size;
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

        /* Some of the cases above might have resulted in an imbalance. */
        result = internal_rebalance(result);
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
        /* A right child exists. It'll be larger than this node. */
        node->right = internal_unlink_max(tree, node->right, max);

        /* It's possible that the left was already one higher than right */
        node = internal_rebalance(node);
        return node;
    }
    else if (node->left) {
        /* No right child exists. This node is maximal; promote left child. */
        *max = node;

        /* No possibility of imbalance. There's was no right child. */
        node = node->left;

        max[0]->left = NULL;
        max[0]->right = NULL;
        max[0]->height = 0;

        return node;
    }
    else {
        /* No children exist. This node's vacuously maximal. */
        *max = node;

        /* No possibility of imbalance. There were no children at all. */
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

static int height (internal_node * node)
{
    if (!node) {
        return -1;
    }
    else {
        return node->height;
    }
}

static internal_node * internal_rebalance (
        internal_node * node
        )
{
    if (node) {

        int left_height = height(node->left);
        int right_height = height(node->right);

        if (right_height - left_height > 1) {
            assert(node->right != NULL);
            /* Height discrepancy too much. Need to rebalance. */
            if (height(node->right->right) > height(node->right->left)) {
                node = rotate_with_right_child(node);
            }
            else if (height(node->right->right) < height(node->right->left)) {
                node = double_with_right_child(node);
            }
            else {
                /* This case happens only during removal. */
                node = rotate_with_right_child(node);
            }
        }
        else if (left_height - right_height > 1) {
            assert(node->left != NULL);
            /* Height discrepancy too much. Need to rebalance. */
            if (height(node->left->left) > height(node->left->right)) {
                node = rotate_with_left_child(node);
            }
            else if (height(node->left->left) < height(node->left->right)) {
                node = double_with_left_child(node);
            }
            else {
                /* This case happens only during removal. */
                node = rotate_with_left_child(node);
            }
        }

        node->height = max(height(node->left), height(node->right)) + 1;
    }

    return node;
}

static internal_node * rotate_with_left_child (
        internal_node * k2
        )
{
    internal_node * k1 = k2->left;
    k2->left = k1->right;
    k1->right = k2;

    k2->height = max(height(k2->left), height(k2->right)) + 1;
    k1->height = max(height(k1->left), height(k1->right)) + 1;

    return k1;
}

static internal_node * double_with_left_child (
        internal_node * k3
        )
{
    k3->left = rotate_with_right_child(k3->left);
    return rotate_with_left_child(k3);
}

static internal_node * rotate_with_right_child (
        internal_node * k1
        )
{
    internal_node * k2 = k1->right;
    k1->right = k2->left;
    k2->left = k1;

    k1->height = max(height(k1->left), height(k1->right)) + 1;
    k2->height = max(height(k2->left), height(k2->right)) + 1;

    return k2;
}

static internal_node * double_with_right_child (
        internal_node * k1
        )
{
    k1->right = rotate_with_left_child(k1->right);
    return rotate_with_right_child(k1);
}

static void internal_foreach (
        internal_node * node,
        foreach_func func
        )
{
    if (node->left) {
        internal_foreach(node->left, func);
    }
    func(node);
    if (node->right) {
        internal_foreach(node->right, func);
    }
}

static int address_compare_func (
        tree_map_key_t left,
        tree_map_key_t right)
{
    uintptr_t left_addr = (uintptr_t)left;
    uintptr_t right_addr = (uintptr_t)right;

    /* Doing explicit comparisons to avoid wraparound on uint math */
    if (left_addr < right_addr) {
        return -1;
    }
    else if (left_addr > right_addr) {
        return 1;
    }
    else {
        return 0;
    }
}

/**
 * Public API name for 'address_compare_func'
 */
tree_map_compare_func tree_map_address_compare_func = &address_compare_func;