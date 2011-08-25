#ifndef __TREE_MAP__
#define __TREE_MAP__

/**
 * Opaque forward declaration
 */
struct tree_map;

/**
 * Key types
 */
typedef void * tree_map_key_t;

/**
 * Value types
 */
typedef void * tree_map_value_t;

/**
 * Key comparison function
 *
 * Return -1 if @left is less than @right, 0 if @left is
 * equal to @right, and +1 if @left is greater than @right.
 */
typedef int (*tree_map_compare_func) (
        tree_map_key_t left,
        tree_map_key_t right
        );

/**
 * Make a tree instance
 */
extern struct tree_map * tree_map_alloc (
        tree_map_compare_func comparator
        );

/**
 * Destroy a tree instance.
 */
extern void tree_map_free (struct tree_map * tree);

/**
 * Maps @key to @value.
 *
 * Returns any previous value that was mapped to @key.
 */
extern tree_map_value_t tree_map_insert (
        struct tree_map * tree,
        tree_map_key_t key,
        tree_map_value_t value
        );

/**
 * Removes any mapping from @key to a value.
 *
 * Returns the value (if any) that was mapped to @key.
 */
extern tree_map_value_t tree_map_remove (
        struct tree_map * tree,
        tree_map_key_t key
        );

/**
 * Finds the value (if any) mapped to @key
 */
extern tree_map_value_t tree_map_lookup (
        struct tree_map * tree,
        tree_map_key_t key
        );

#endif /* __TREE_MAP__ */
