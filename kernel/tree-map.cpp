#include <muos/spinlock.h>

#include <kernel/assert.h>
#include <kernel/minmax.hpp>
#include <kernel/object-cache.hpp>
#include <kernel/once.h>
#include <kernel/tree-map.hpp>

static Once_t init_control = ONCE_INIT;

/* Allocates internal_node's */
static struct ObjectCache   internal_node_cache;
static Spinlock_t           internal_node_cache_lock = SPINLOCK_INIT;

/* Allocates tree_map's */
static struct ObjectCache   tree_map_cache;
static Spinlock_t           tree_map_cache_lock = SPINLOCK_INIT;

class RawTreeMap::InternalNode
{
public:
    InternalNode * left;
    InternalNode * right;
    int            height;

    Key_t     key;
    Value_t   value;

public:
    static InternalNode * Insert (
            RawTreeMap * tree,
            InternalNode * node,
            Key_t key,
            Value_t value,
            Value_t * prev_value
            );

    static InternalNode * Remove (
            RawTreeMap * tree,
            InternalNode * node,
            Key_t key,
            Value_t * prev_value
            );
};

typedef RawTreeMap::InternalNode   InternalNode;
typedef RawTreeMap::Key_t          Key_t;
typedef RawTreeMap::Value_t        Value_t;

/* Needs to be signed integer to avoid wraparound when comparing results */
static int height (
        InternalNode * node
        );

static InternalNode * internal_unlink_max (
        struct RawTreeMap * tree,
        InternalNode * node,
        InternalNode ** max
        );

static void internal_free_node (
        struct RawTreeMap * tree,
        InternalNode * node
        );

static InternalNode * internal_rebalance (
        InternalNode * node
        );

static InternalNode * rotate_with_left_child (
        InternalNode * k2
        );

static InternalNode * double_with_left_child (
        InternalNode * k3
        );

static InternalNode * rotate_with_right_child (
        InternalNode * k1
        );

static InternalNode * double_with_right_child (
        InternalNode * k1
        );

typedef void (*foreach_func) (
        InternalNode * node,
        void * user_data
        );

static void internal_foreach (
        InternalNode * node,
        foreach_func func,
        void * user_data
        );

/**
 * One-time initialization before any trees can be created.
 */
static void tree_map_static_init (void * param)
{
    SpinlockLock(&internal_node_cache_lock);
    ObjectCacheInit(&internal_node_cache, sizeof(InternalNode));
    SpinlockUnlock(&internal_node_cache_lock);

    SpinlockLock(&tree_map_cache_lock);
    ObjectCacheInit(&tree_map_cache, sizeof(struct RawTreeMap));
    SpinlockUnlock(&tree_map_cache_lock);
}

void * RawTreeMap::operator new (size_t size) throw (std::bad_alloc)
{
    void * result;

    assert(size == sizeof(RawTreeMap));
    Once(&init_control, tree_map_static_init, NULL);

    SpinlockLock(&tree_map_cache_lock);
    result = ObjectCacheAlloc(&tree_map_cache);
    SpinlockUnlock(&tree_map_cache_lock);

    if (result != NULL) {
        return result;
    } else {
        throw std::bad_alloc();
    }
}

RawTreeMap::RawTreeMap (RawTreeMap::CompareFunc comparator) throw ()
{
    this->root = NULL;
    this->comparator = comparator;
    this->size = 0;
}

void RawTreeMap::operator delete (void * mem) throw ()
{
    assert(mem != NULL);

    SpinlockLock(&tree_map_cache_lock);
    ObjectCacheFree(&tree_map_cache, mem);
    SpinlockUnlock(&tree_map_cache_lock);
}

RawTreeMap::~RawTreeMap () throw ()
{
    internal_free_node(this, this->root);
}

static void check_subtree_balance (
        InternalNode * node,
        void * user_data
        )
{
    int balance = height(node->left) - height(node->right);

    assert(balance >= -1 && balance <= 1);
}

Value_t RawTreeMap::Insert (
        Key_t key,
        Value_t value
        )
{
    Value_t prev_value = NULL;

    assert(this != NULL);
    this->root = InternalNode::Insert(this, this->root, key, value, &prev_value);
    internal_foreach(this->root, check_subtree_balance, NULL);

    if (!prev_value) {
        this->size++;
    }

    return prev_value;
}

Value_t RawTreeMap::Remove (
        Key_t key
        )
{
    Value_t prev_value = NULL;

    assert(this != NULL);
    this->root = InternalNode::Remove(this, this->root, key, &prev_value);

    if (prev_value) {
        this->size--;
    }

    return prev_value;
}

Value_t RawTreeMap::Lookup (
        Key_t key
        )
{
    InternalNode * node;
    int compare_val;

    assert(this != NULL);
    node = this->root;
    compare_val = node ? this->comparator(key, node->key) : 0;

    while (node && compare_val) {
        node = compare_val < 0 ? node->left : node->right;
        compare_val = node ? this->comparator(key, node->key) : 0;
    }

    return node ? node->value : NULL;
}

unsigned int RawTreeMap::Size ()
{
    assert(this != NULL);
    return this->size;
}

struct closure
{
    RawTreeMap::ForeachFunc user_func;
    void * user_data;
};

static void user_data_visitor (InternalNode * node, void * user_data)
{
    struct closure * closure = (struct closure *)user_data;

    if (node) {
        closure->user_func(node->key, node->value, closure->user_data);
    }
}

void RawTreeMap::Foreach (
        ForeachFunc func,
        void * user_data
        )
{
    struct closure c = { func, user_data };

    assert(this != NULL);
    internal_foreach(this->root, user_data_visitor, &c);
}

InternalNode * InternalNode::Insert (
        RawTreeMap * tree,
        InternalNode * node,
        Key_t key,
        Value_t value,
        Value_t * prev_value
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
            node->right = InternalNode::Insert(tree, node->right, key, value, prev_value);
            node = internal_rebalance(node);
            internal_foreach(node->right, check_subtree_balance, NULL);

            return node;
        }
        else {
            /* New key is less than this node. */
            node->left = InternalNode::Insert(tree, node->left, key, value, prev_value);
            node = internal_rebalance(node);
            internal_foreach(node->left, check_subtree_balance, NULL);
            return node;
        }
    }
    else {
        SpinlockLock(&internal_node_cache_lock);
        InternalNode * new_node = (InternalNode *)ObjectCacheAlloc(&internal_node_cache);
        SpinlockUnlock(&internal_node_cache_lock);

        new_node->left = NULL;
        new_node->right = NULL;
        new_node->height = 0;
        new_node->key = key;
        new_node->value = value;
        *prev_value = NULL;

        return new_node;
    }
}

InternalNode * InternalNode::Remove (
        RawTreeMap * tree,
        InternalNode * node,
        Key_t key,
        Value_t * prev_value
        )
{
    InternalNode * result;

    if (node) {
        int compare_val = tree->comparator(key, node->key);

        if (compare_val == 0) {
            /* Removing this node */
            *prev_value = node->value;

            if (!node->left && !node->right) {
                /* Both subtrees are empty */
                SpinlockLock(&internal_node_cache_lock);
                ObjectCacheFree(&internal_node_cache, node);
                SpinlockUnlock(&internal_node_cache_lock);

                result = NULL;
            }
            else if (node->left && !node->right) {
                /* Only left subtree is nonempty */
                result = node->left;

                SpinlockLock(&internal_node_cache_lock);
                ObjectCacheFree(&internal_node_cache, node);
                SpinlockUnlock(&internal_node_cache_lock);
            }
            else if (!node->left && node->right) {
                /* Only right subtree is nonempty */
                result = node->right;

                SpinlockLock(&internal_node_cache_lock);
                ObjectCacheFree(&internal_node_cache, node);
                SpinlockUnlock(&internal_node_cache_lock);
            }
            else {
                /*
                Both subtrees are nonempty. Promote the maximum element
                of the left subtree to be the new root node.
                */
                node->left = internal_unlink_max(tree, node->left, &result);
                result->left = node->left;
                result->right = node->right;

                SpinlockLock(&internal_node_cache_lock);
                ObjectCacheFree(&internal_node_cache, node);
                SpinlockUnlock(&internal_node_cache_lock);
            }
        }
        else if (compare_val > 0) {
            /* Key is greater than us. If a match exists, it's in right subtree. */
            node->right = InternalNode::Remove(tree, node->right, key, prev_value);

            // XXX: rebalance
            result = node;
        }
        else {
            /* Key is less than us. If a match exists, it's in left subtree. */
            node->left = InternalNode::Remove(tree, node->left, key, prev_value);

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
static InternalNode * internal_unlink_max (
        struct RawTreeMap * tree,
        InternalNode * node,
        InternalNode ** max
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
        struct RawTreeMap * tree,
        InternalNode * node
        )
{
    if (node) {
        internal_free_node(tree, node->left);
        internal_free_node(tree, node->right);

        SpinlockLock(&internal_node_cache_lock);
        ObjectCacheFree(&internal_node_cache, node);
        SpinlockUnlock(&internal_node_cache_lock);
    }
}

static int height (InternalNode * node)
{
    if (!node) {
        return -1;
    }
    else {
        return node->height;
    }
}

static InternalNode * internal_rebalance (
        InternalNode * node
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

        node->height = MAX(height(node->left), height(node->right)) + 1;
    }

    return node;
}

static InternalNode * rotate_with_left_child (
        InternalNode * k2
        )
{
    InternalNode * k1 = k2->left;
    k2->left = k1->right;
    k1->right = k2;

    k2->height = MAX(height(k2->left), height(k2->right)) + 1;
    k1->height = MAX(height(k1->left), height(k1->right)) + 1;

    return k1;
}

static InternalNode * double_with_left_child (
        InternalNode * k3
        )
{
    k3->left = rotate_with_right_child(k3->left);
    return rotate_with_left_child(k3);
}

static InternalNode * rotate_with_right_child (
        InternalNode * k1
        )
{
    InternalNode * k2 = k1->right;
    k1->right = k2->left;
    k2->left = k1;

    k1->height = MAX(height(k1->left), height(k1->right)) + 1;
    k2->height = MAX(height(k2->left), height(k2->right)) + 1;

    return k2;
}

static InternalNode * double_with_right_child (
        InternalNode * k1
        )
{
    k1->right = rotate_with_left_child(k1->right);
    return rotate_with_right_child(k1);
}

static void internal_foreach (
        InternalNode * node,
        foreach_func func,
        void * user_data
        )
{
    if (node) {
        internal_foreach(node->left, func, user_data);
        func(node, user_data);
        internal_foreach(node->right, func, user_data);
    }
}

static int address_compare_func (
        Key_t left,
        Key_t right)
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

static int signed_int_compare_func (
        Key_t left,
        Key_t right
        )
{
    return (int)left - (int)right;
}

/**
 * Public API name for 'address_compare_func'
 */
RawTreeMap::CompareFunc RawTreeMap::AddressCompareFunc = &address_compare_func;

/**
 * Public API name for 'signed_int_compare_func'
 */
RawTreeMap::CompareFunc RawTreeMap::SignedIntCompareFunc = &signed_int_compare_func;
