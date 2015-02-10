#ifndef CB_TREE_H
#define CB_TREE_H

#include <stdint.h>
#include <pthread.h>

#define KEYLEN 32 // 256 bits

#define TYPE_BRANCH 0
#define TYPE_LEAF 1

#define SUCCESS 1
#define FAIL 0

// branch node
typedef struct cb_branch{
	uint8_t		type:1;		// node type (branch or leaf)
	uint8_t		byte:5; 	// which key byte defines the sons' direction
	uint8_t		bitmask; 	// which bit in the key bite defines the sons' direction
//	pthread_mutex_t lock;		// in case you want to use mutex instead
	pthread_spinlock_t lock;	// node's lock
	void*		son[2];		// 2 sons
} cb_branch;

// leaf node
typedef struct cb_leaf{
	uint8_t		type:1;		// node type (branch or leaf)
	uint8_t		key[KEYLEN];	// leaf's key
	void		*data;		// Pointer to attach your data
} cb_leaf;

// tree's root. The beginning of everything.
typedef struct{
	cb_branch*	root;
} cb_tree;

// the tree
cb_tree *critbit;

// initializes the mask table
void
cb_mask_table_init();

// computes the crit bit and byte for a branch node hanging two leafs
void 
cb_crit_bit(cb_leaf *o1, cb_leaf *o2, cb_branch *n);

// initalizes the tree
void 
cb_init();

// inserts a leaf node into the tree
cb_leaf* 
cb_insert(cb_leaf *obj, uint32_t *retries);

// finds a leaf node in the tree
cb_leaf* 
cb_find(uint8_t *key, uint32_t *retries);

// removes a leaf node from the tree
int 
cb_remove(uint8_t *key, uint32_t *retries);

// prints the tree and returns the numbers of leafs
uint64_t 
cb_print(void *p);

// auxiliary to cb_print
void 
_cb_print(void *p, uint64_t *n_nodes, uint64_t *n_objs, int _tab);

#endif
