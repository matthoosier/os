#ifndef __TREE_MAP__
#define __TREE_MAP__

/**
 * Opaque forward declaration
 */
struct TreeMap;

/**
 * Key types
 */
typedef void * TreeMapKey_t;

/**
 * Value types
 */
typedef void * TreeMapValue_t;

/**
 * Key comparison function
 *
 * Return -1 if @left is less than @right, 0 if @left is
 * equal to @right, and +1 if @left is greater than @right.
 */
typedef int (*TreeMapCompareFunc) (
        TreeMapKey_t left,
        TreeMapKey_t right
        );

typedef void (*TreeMapForeachFunc) (
        TreeMapKey_t key,
        TreeMapValue_t value,
        void * user_data
        );

/**
 * Canned key-comparision function suitable for comparing
 * keys that are virtual memory addresses.
 */
extern TreeMapCompareFunc TreeMapAddressCompareFunc;

/**
 * Make a tree instance
 */
extern struct TreeMap * TreeMapAlloc (
        TreeMapCompareFunc comparator
        );

/**
 * Destroy a tree instance.
 */
extern void TreeMapFree (struct TreeMap * tree);

/**
 * Maps @key to @value.
 *
 * Returns any previous value that was mapped to @key.
 */
extern TreeMapValue_t TreeMapInsert (
        struct TreeMap * tree,
        TreeMapKey_t key,
        TreeMapValue_t value
        );

/**
 * Removes any mapping from @key to a value.
 *
 * Returns the value (if any) that was mapped to @key.
 */
extern TreeMapValue_t TreeMapRemove (
        struct TreeMap * tree,
        TreeMapKey_t key
        );

/**
 * Finds the value (if any) mapped to @key
 */
extern TreeMapValue_t TreeMapLookup (
        struct TreeMap * tree,
        TreeMapKey_t key
        );

/**
 * Returns number of entries in the map
 */
extern unsigned int TreeMapSize (
        struct TreeMap * tree
        );

/**
 * Visit each key/value pair current stored in the map
 */
extern void TreeMapForeach (
        struct TreeMap * tree,
        TreeMapForeachFunc func,
        void * user_data
        );

#endif /* __TREE_MAP__ */
