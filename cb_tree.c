#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "cb_tree.h"

// defines VERBOSE_TEST
#ifdef VERBOSE_ON
	#define VERBOSE_TEST 1
#else
	#define VERBOSE_TEST 0
#endif

// defines DEBUG_TEST
#ifdef DEBUG_ON
	#define DEBUG_TEST 1
#else
	#define DEBUG_TEST 0
#endif

// defines verbose macro
#define verbose(msg, ...)\
do{\
	if(VERBOSE_TEST || DEBUG_TEST){\
		fprintf(stdout, " - " msg "\n", ##__VA_ARGS__);\
		fflush(stdout);\
	}\
} while(0)

// defines debug macro
#define debug(msg, ...)\
do{\
	if(DEBUG_TEST){\
		fprintf(stdout, "[debug] %s:%d|%s() - " msg "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);\
		fflush(stdout);\
	}\
} while(0)

// defines whether using spinlock or mutex
#define SPINLOCK
#ifdef SPINLOCK
	#define LOCK(lock) pthread_spin_lock(lock)
	#define UNLOCK(lock) pthread_spin_unlock(lock)
	#define LOCK_INIT(lock) pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
#else
	#define LOCK(lock) pthread_mutex_lock(lock)
	#define UNLOCK(lock) pthread_mutex_unlock(lock)
	#define LOCK_INIT(lock) pthread_mutex_init(lock, NULL)
#endif

// increments the counter and continues (retries)
#define retrying()\
		if(retries)(*retries)++;\
		continue

// retrieves the precoputed crit bit mask from the mask table
#define cb_bit(byte1, byte2)\
	mask_table[(byte1) ^ (byte2)]

extern cb_tree* critbit;	// the tree
uint8_t mask_table[256];	// the mask table

// precomputes bitmasks for every possible combination of byte1 and byte2, so we don't need to calculate it later
void cb_mask_table_init(){
	uint8_t byte1, byte2, bit, mask;
	byte1 = 0;
	while(1){
		byte2 = 0;
		while(1){
			mask = 1;
			for(bit = 0; bit < 8 && ((byte1 & mask) == (byte2 & mask)); bit++, mask <<= 1);
			mask_table[byte1 ^ byte2] = mask;
			byte2++;
			if(byte2 == 0) break;
		}
		byte1++;
		if(byte1 == 0) break;
	}
}

// Recebe dois objetos e, baseado nas suas chaves, define as informacoes de roteamento do nodo auxiliar que ligara os dois objetos.
// Estas informacoes sao o numero do byte da chave que contem o bit de diferencia os objetos, e o numero do bit dentro deste byte.
inline void cb_crit_bit(cb_leaf *o1, cb_leaf *o2, cb_branch *n){
	uint8_t byte;
	for(byte = 0; byte < KEYLEN && o1->key[byte] == o2->key[byte]; byte++);
	n->byte = byte;
	n->bitmask = cb_bit(o1->key[byte], o2->key[byte]);
}

// Inicializa a arvore com 2 objetos falsos. O objetivo eh evitar alguns casos especificos de insercoes e remocoes, que soh acontecem quando a arvore contem 0 ou 1 ojeto.
// Pode-se estudar a inicializacoes da arvore com 4 objetos falsos, ou mesmo a eliminacao desta inicializacao, caso julgue-se desnecessario.
void cb_init(){
	
	cb_mask_table_init();
	
	critbit = (cb_tree*) calloc(1, sizeof(cb_tree));
	
	cb_leaf *leaf1 = (cb_leaf*) malloc(sizeof(cb_leaf));
	cb_leaf *leaf2 = (cb_leaf*) malloc(sizeof(cb_leaf));
	cb_branch *branch = (cb_branch*) malloc(sizeof(cb_branch));
	debug("Initial memory allocated");

	leaf1->type = TYPE_LEAF;
	leaf2->type = TYPE_LEAF;
	branch->type = TYPE_BRANCH;

	uint8_t key[KEYLEN];
	memset(key, 0, KEYLEN);

	memcpy(leaf1->key, key, KEYLEN);
	key[0] = 1;
	memcpy(leaf2->key, key, KEYLEN);

	cb_crit_bit(leaf1, leaf2, branch);
	uint8_t direction = (leaf1->key[branch->byte] & branch->bitmask) != 0;
	branch->son[direction] = leaf1;
	branch->son[1 - direction] = leaf2;
	debug("Initial nodes set up");
	LOCK_INIT(&(branch->lock));
	debug("Initial lock set up");
	critbit->root = branch;

	verbose("Tree initialized successfully.");
}

cb_leaf* cb_insert(cb_leaf *leaf, uint32_t *retries){
	
	cb_branch *p, *f, *gf; // ponteiros para objeto, pai e avo
	uint8_t s_direction, f_direction, gf_direction;
	
	cb_branch *new_father = (cb_branch*) malloc(sizeof(cb_branch)); // nodo auxiliar
	if(!new_father){
		debug("malloc() fail");
		return NULL;
	}
	
// Initalizing some values before finding the position and before locking. Makes de time spent after findind a position and inside the critical section shorter, therefore there will be less concurrency.
	leaf->type = TYPE_LEAF;
	new_father->type = TYPE_BRANCH;
	LOCK_INIT(&(new_father->lock));

	while(1){
		f = NULL; gf = NULL; gf_direction = 0; f_direction = 0;
		p = critbit->root;
//		debug("Walking through the tree.");
		while(p->type == TYPE_BRANCH){ // caminha pela arvore
			gf_direction = f_direction;
			f_direction = (leaf->key[p->byte] & p->bitmask) != 0; // decide a direcao
			gf = f;
			f = p;
			p = p->son[f_direction];
			if(p == NULL){
//				debug("Position invalidated by a concurrent exclusion. Restarting the search.");
				f = NULL; gf = NULL; gf_direction = 0; f_direction = 0; p = critbit->root;
				retrying();
			}
		}
//		debug("Found the position.");
		// Encontrou a posicao. Se o nodo jah existe e o novo objeto nao e' vary, nao insere e retorna NULL.
		if(!memcmp(leaf->key, ((cb_leaf*)p)->key, KEYLEN)){
			free(new_father);
//			debug("Occupied position. Key is already in the tree.");
			return NULL;
		}

		// Locks no avo (se existente) e no pai.
		if(gf){
			LOCK(&(gf->lock));
//			debug("Grandfather's lock obtained.");
			if(gf->son[gf_direction] != f){
//				debug("Link Grandfather -> Father lost.");
				UNLOCK(&(gf->lock));
//				debug("Grandfather's lock released.");
//				debug("Object not inserted. Trying again.");
				retrying();
			}
		}
		
		LOCK(&(f->lock));
//		debug("Father's lock obtained");
		if(f->son[f_direction] != p){
//			debug("Link Father -> Son lost.");
			UNLOCK(&(f->lock));
//			debug("Father's lock released.");
			if(gf){
				UNLOCK(&(gf->lock));
//				debug("Grandfather's lock released.");
			}
//			debug("Object not inserted. Trying again.");
			retrying();
		}
//		debug("Setting up new leaf.");

		// Tudo certo. Inicializamos o nodo auxiliar com as informacoes de roteamento, ligamos os objetos a ele, inicializamos o seu lock e inserimos ele na árvore com CAS.
		// Se o CAS falhar, significa que tivemos uma modificacao naquele ponto da arvore, e temos que procurar a nova posicao a inserir o objeto.
		cb_crit_bit(leaf, (cb_leaf*)p, new_father);
		s_direction = (leaf->key[new_father->byte] & new_father->bitmask) != 0;
		new_father->son[s_direction] = leaf;
		new_father->son[1 - s_direction] = p;
		f->son[f_direction] = new_father;

		UNLOCK(&(f->lock));
		if(gf) UNLOCK(&(gf->lock));
//		debug("Both locks released.");

//		verbose("New object inserted successfully.");
		return leaf;
	}
}


cb_leaf *cb_find(uint8_t *key, uint32_t *retries){
	cb_branch *p = critbit->root;
//	debug("Walking through the tree.");
	while(p->type == TYPE_BRANCH){ // Caminhamento pela arvore
		p = p->son[(key[p->byte] & p->bitmask) != 0];
		if(p == NULL){ // Posicao invalidada por uma remocao paralela;
//			debug("Position invalidated by a concurrent exclusion. Restarting the search.");
			p = critbit->root; // Reinicia a busca.
			retrying();
		}
	}

	// A posicao encontrada contem o objeto que procuramos?
	if(!memcmp(key, ((cb_leaf*)p)->key, KEYLEN)){
//		debug("Found the leaf.");
//		verbose("Object found.");
		return (cb_leaf*)p;
	}
	
//	verbose("Object not found.");
	return NULL;
}

// A remocao eh parecida com a insercao.
// Basicamente, caminhamos pela arvore ate encontrar o objeto. Encontrado este, adquirimos os locks do pai e do avo, verificamos a consistencia da ligacao do pai com o objeto e entao apontamos o avo para o objeto irmao. Dessa maneira, excluimos da arvore o objeto desejado e o nodo auxiliar (pai) que ligava os dois objetos. Agora, o avo tornou-se pai do objeto irmao.
int cb_remove(uint8_t *key, uint32_t *retries){
	cb_branch *p, *f, *gf;
	cb_leaf *return_leaf;
	uint8_t f_direction, gf_direction;
		
	while(1){
		f = NULL; gf = NULL;
		p = critbit->root;
//		debug("Walking through the tree.");
		while(p->type == TYPE_BRANCH){
			gf_direction = f_direction;
			f_direction = (key[p->byte] & p->bitmask) != 0;
			gf = f;
			f = p;
			p = p->son[f_direction];
			if(p == NULL){
//				debug("Position invalidated by a concurrent exclusion. Restarting the search.");
				f = NULL; gf = NULL; p = critbit->root;
				retrying();
			}
		}
//		debug("Found the position.");

		if(memcmp(key, ((cb_leaf*)p)->key, KEYLEN) != 0){
//			debug("Object not found.");
			return FAIL;
		}

		LOCK(&(gf->lock));
//		debug("Grandfather's lock obtained.");
		if(gf->son[gf_direction] != f){
//			debug("Link Grandfather -> Father lost.");
			UNLOCK(&(gf->lock));
//			debug("Grandfather's lock released.");
//			debug("Leaf not removed. Trying again.");
			retrying();
		}

		LOCK(&(f->lock));
//		debug("Father's lock obtained.");
		if(f->son[f_direction] != p){
//			debug("Link Father -> Son lost.");
			UNLOCK(&(f->lock));
//			debug("Father's lock released.");
			UNLOCK(&(gf->lock));
//			debug("Grandfather's lock released.");
//			debug("Leaf not removed. Trying again.");
			retrying();
		}

		gf->son[gf_direction] = f->son[1 - f_direction];
		f->son[0] = NULL;
		f->son[1] = NULL;
		
		UNLOCK(&(f->lock));
		UNLOCK(&(gf->lock));
//		debug("Both locks released.");

		/* TODO: Garbage Collection
		 * We can't free this memory.
		 */
//		free(f);
//		free(p);
			
//		verbose("Object successfully removed.");
		return SUCCESS;
	}
}

void _cb_print(void *p, uint64_t *n_nodes, uint64_t *n_objs, int _tab){
	
	if(!p) return;

	cb_branch *n;
//	cb_leaf *o;
	int i;
/*
 	int tab = _tab;
	for(;tab>0;tab--)
		printf("\t");
*/
	if(((cb_branch*)p)->type == TYPE_BRANCH){
		//(*n_nodes)++;
		n = (cb_branch*)p;
/*
		printf("--- NODO (%p), ", n);
		printf("byte: %u, ", n->byte);
		//printf("critbit: %u, ", n->critbit);
//		printf("bitmask: %s, ", bin_print(n->bitmask));
		printf("son[0]: %p, son[1]: %p\n", n->son[0], n->son[1]);
*/
		for(i=0; i<2; i++)
			_cb_print(n->son[i], n_nodes, n_objs, _tab+1);
	}
	else{
		(*n_objs)++;
//		o = (cb_leaf*)p;
/*
		printf("--- OBJETO (%p), ", o);
		printf("key: %s\n", o->key);
*/
	}
}

// Percorre recursivamente a arvore.
// Tirando os comentarios, a arvore eh impressa. Mantendo-os, a funcao apenas retorna o numero de objetos presentes.
uint64_t cb_print(void *p){
	uint64_t n_nodes = 0, n_objs = 0;
	_cb_print(p, &n_nodes, &n_objs, 0);
	//printf("\n%lu nodos intermediarios.\n", n_nodes);
	return n_objs-2;
}
