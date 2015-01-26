#ifndef CB_TREE_H
#define CB_TREE_H

#include <stdint.h>
#include <pthread.h>
//#include "vsha256.h"

//#define KEYLEN SHA256_LEN
#define KEYLEN 32

#define TYPE_BRANCH 0
#define TYPE_LEAF 1

#define SUCCESS 1
#define FAIL 0

typedef struct cb_branch{
	uint8_t		type:1; // node type (branch or leaf)
	uint8_t		byte:5; // numero do byte da chave que diferencia as subarvores filhas
	uint8_t		bitmask; // bitmask correspondente ao bit que diferencia as subarvores
//	pthread_mutex_t lock;
	pthread_spinlock_t lock;
	void*		son[2];
} cb_branch;

typedef struct cb_leaf{
	uint8_t		type:1;
	uint8_t		key[KEYLEN];
	void		*data;
} cb_leaf;

typedef struct{
	cb_branch*	root;
} cb_tree;

cb_tree *critbit;

void
cb_mask_table_init();

void 
cb_crit_bit(cb_leaf *o1, cb_leaf *o2, cb_branch *n);

void 
cb_init();

cb_leaf* 
cb_insert(cb_leaf *obj, uint32_t *retries);

cb_leaf* 
cb_find(uint8_t *key, uint32_t *retries);

int 
cb_remove(uint8_t *key, uint32_t *retries);

void 
_cb_print(void *p, uint64_t *n_nodes, uint64_t *n_objs, int _tab);

uint64_t 
cb_print(void *p);

#endif
