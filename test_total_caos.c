/*
 * Total Caos test.
 * Insertions, searches and removes all happening at the same time.
 * You choose how many total threads, but their operation is randonly chosen every time.
 * Random keys ranging between 0 and KEY_VAY (1000000)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "cb_tree.h"
#include "almeidamacros.h"

#define KEY_VAR 1000000
#define OP_VAR 3

extern cb_tree* critbit;
uint32_t *retries;

// 64 randomly generated seeds
unsigned int seeds[64] = {1973412707, 1422567173, 1425107014, 1732354787, 1266399658, 1437824, 1346953959, 1617318107, 239535387, 1036416839, 585651951, 1935751300, 1093957290, 1636332718, 109029321, 761690412, 414069420, 1685936890, 1438113585, 648763923, 18572912, 1796836112, 1264335488, 1987265457, 709860657, 1328358350, 1818800135, 1602883452, 1234751059, 2016848860, 16397140, 1060680118, 1291932386, 1441504154, 645551257, 410848396, 1442941978, 1992505216, 2028166503, 1682477365, 881438408, 466334807, 1470745018, 1975395698, 2102667525, 1579774339, 589602462, 369253297, 1118227582, 2027716048, 1018017220, 1136800494, 1677068512, 134869060, 976582303, 239445522, 1463227410, 647898790, 1842328974, 550494821, 517264003, 1858726114, 1611174940};

void *thread_function(void *index){
	int tindex = *(int*)index;

	cb_leaf *leaf;
	unsigned int rand_state = seeds[tindex];
	int rand_n;
	char key[KEYLEN];

	while(1){
		rand_n = rand_r(&rand_state);

		switch(rand_n%OP_VAR){
			case 0:
				// Search
				memset(key, 0, KEYLEN);
				sprintf(key, "%d", rand_n%KEY_VAR);
				cb_find(key, &retries[tindex]);
				break;
			case 2:
				// Insert
				leaf = talloc(cb_leaf, 1);
				memset(leaf->key, 0, KEYLEN);
				sprintf(leaf->key, "%d", rand_n%KEY_VAR);
				cb_insert(leaf, &retries[tindex]);
				break;
			case 1:
				// Remove
				memset(key, 0, KEYLEN);
				sprintf(key, "%d", rand_n%KEY_VAR);
				cb_remove(key, &retries[tindex]);
				break;
		}
	}
}

void *count_function(){
	while(1){
		verbose("%ld objects in the tree.", cb_print(critbit->root));
		sleep(1);
	}
}

int main(int argc, char **argv){

	if(argc < 3){
		verbose("Usage: ./test execution_time(seconds) #threads");
		return;
	}

	int execution_time = atoi(argv[1]);
	int nthreads = atoi(argv[2]);

	verbose("Execution time: %ds.", execution_time);
	verbose("Number of threads: %d.", nthreads);
	
	cb_init();

	pthread_t *threads = talloc(pthread_t, nthreads);
	int *thread_index = talloc(int, nthreads);
	retries = talloc(uint32_t, nthreads);
	memset(retries, 0, sizeof(uint32_t)*nthreads);
	int i, nt = 0;

/* 
 * THREADS
 */
	for(i = 0; i < nthreads; i++){
		thread_index[i] = i;
		if(pthread_create(&threads[i], NULL, &thread_function, &thread_index[i]))
			error("Creation of thread #%d failed.", i);
		else nt++;
	}
	verbose("%d threads created successfully.", nt);

/*
 * COUNTING OBJECTS
 */
	pthread_t count_thread;
	pthread_create(&count_thread, NULL, &count_function, NULL);

	sleep(execution_time);

	uint32_t total_retries = 0;
	for(i = 0; i <  nthreads; i++)
		total_retries += retries[i];
	verbose("Total number of retries: %d", total_retries);
}
