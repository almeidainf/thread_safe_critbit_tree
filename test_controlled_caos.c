/*
 * Controlled Caos test.
 * Insertions, searches and removes all happening at the same time.
 * You choose how many threads for each operation.
 * Random keys ranging between 0 and KEY_VAR (1000000)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "cb_tree.h"
#include "almeidamacros.h"

#define KEY_VAR 1000000

extern cb_tree* critbit;

// 64 randomly generated seeds
unsigned int seeds[64] = {1973412707, 1422567173, 1425107014, 1732354787, 1266399658, 1437824, 1346953959, 1617318107, 239535387, 1036416839, 585651951, 1935751300, 1093957290, 1636332718, 109029321, 761690412, 414069420, 1685936890, 1438113585, 648763923, 18572912, 1796836112, 1264335488, 1987265457, 709860657, 1328358350, 1818800135, 1602883452, 1234751059, 2016848860, 16397140, 1060680118, 1291932386, 1441504154, 645551257, 410848396, 1442941978, 1992505216, 2028166503, 1682477365, 881438408, 466334807, 1470745018, 1975395698, 2102667525, 1579774339, 589602462, 369253297, 1118227582, 2027716048, 1018017220, 1136800494, 1677068512, 134869060, 976582303, 239445522, 1463227410, 647898790, 1842328974, 550494821, 517264003, 1858726114, 1611174940};

void *insert_function(void *index){
	int tindex = *(int*)index;

	cb_leaf *leaf;
	unsigned int rand_state = seeds[tindex];

	while(1){
		leaf = talloc(cb_leaf, 1);
		memset(leaf->key, 0, KEYLEN);
 		sprintf(leaf->key, "%d", rand_r(&rand_state)%KEY_VAR);
		cb_insert(leaf, NULL);
	}
}

void *find_function(void *index){
	int tindex = *(int*)index;

	char key[KEYLEN];
	unsigned int rand_state = seeds[tindex];

	while(1){
		memset(key, 0, KEYLEN);
		sprintf(key, "%d", rand_r(&rand_state)%KEY_VAR);
		cb_find(key, NULL);
	}
}

void *remove_function(void *index){
	int tindex = *(int*)index;

	char key[KEYLEN];
	unsigned int rand_state = seeds[tindex];

	while(1){
		memset(key, 0, KEYLEN);
		sprintf(key, "%d", rand_r(&rand_state)%KEY_VAR);
		cb_remove(key, NULL);
	}
}

void *count_function(){
	while(1){
		verbose("%ld objects in the tree.", cb_print(critbit->root));
		sleep(1);
	}
}

int main(int argc, char **argv){
	
	if(argc < 5){
		verbose("Usage: ./test execution_time(seconds) #insert_threads #find_threads #remove_threads");
		return;
	}

	int execution_time = atoi(argv[1]);
	int n_insert_th = atoi(argv[2]);
	int n_find_th = atoi(argv[3]);
	int n_remove_th = atoi(argv[4]);

	verbose("Execution time: %ds.", execution_time);
	verbose("%d threads inserting, %d threads searching and %d threads removing.", n_insert_th, n_find_th, n_remove_th);
	
	cb_init();

	pthread_t *threads = talloc(pthread_t, n_insert_th + n_find_th + n_remove_th);
	int *thread_index = talloc(int, n_insert_th + n_find_th + n_remove_th);
	int i;

	int in = 0, fn = 0, rn = 0;
/* 
 * INSERTING OBJECTS
 */
	for(i = 0; i < n_insert_th; i++){
		thread_index[i] = i;
		if(pthread_create(&threads[i], NULL, &insert_function, &thread_index[i]))
			error("Creation of insert thread #%d failed.", i);
		else in++;
	}
	verbose("%d insert threads created successfully.", in);

/*
 * FINDING OBJECTS
 */
	for(; i < (n_insert_th + n_find_th); i++){
		thread_index[i] = i;
		if(pthread_create(&threads[i], NULL, &find_function, &thread_index[i]))
			error("Creation of find thread #%d failed.", i);
		else fn++;
	}
	verbose("%d find threads created successfully.", fn);

/*
 * REMOVING OBJECT
 */
	for(; i < (n_insert_th + n_find_th + n_remove_th); i++){
		thread_index[i] = i;
		if(pthread_create(&threads[i], NULL, &remove_function, &thread_index[i]))
			error("Creation of remove thread #%d failed.", i);
		else rn++;
	}
	verbose("%d remove threads created successfully.", rn);

/*
 * COUNTING OBJECTS
 */
	pthread_t count_thread;
	pthread_create(&count_thread, NULL, &count_function, NULL);

	sleep(execution_time);
}
