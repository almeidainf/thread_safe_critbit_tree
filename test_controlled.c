/*
 * Controlled test.
 * First inserts, then searches, and then removes.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include "cb_tree.h"
#include "almeidamacros.h"

#define time_calc()\
do{\
	secs = (tf.tv_sec - ti.tv_sec);\
	msecs = (tf.tv_usec - ti.tv_usec)/1000;\
	usecs = (tf.tv_usec - ti.tv_usec)%1000;\
	if(msecs < 0){secs--; msecs += 1000;}\
	if(usecs < 0){msecs--; usecs += 1000;}\
} while(0)

extern cb_tree* critbit;
int nthreads;
uint32_t nobjs;

void *thread_insert(void *index){
	int tindex = *(int*)index;

	uint32_t retries = 0;
	cb_leaf *leaf;
	int i;
	for(i = tindex*nobjs; i < (tindex+1)*nobjs; i++){
		leaf = talloc(cb_leaf, 1);
		memset(leaf->key, 0, KEYLEN);
 //sprintf(leaf->key, "chave %d", i);
 		sprintf(leaf->key, "%d%d%d%d", i*4, i*2, i/2, i/4);

		cb_insert(leaf, &retries);
	}
	verbose("Insert thread #%d retried %d times.", tindex, retries);
}

void *thread_find(void *index){
	int tindex = *(int*)index;

	uint32_t retries = 0;
	int found = 0, i;
	char key[KEYLEN];
	for(i = tindex*nobjs; i < (tindex+1)*nobjs; i++){
		memset(key, 0, KEYLEN);
		sprintf(key, "%d%d%d%d", i*4, i*2, i/2, i/4);
		if(cb_find(key, &retries)) found++;
	}

	verbose("Find thread #%d found %d objects, and retried %d times.", tindex, found, retries);
}

void *thread_remove(void *index){
	int tindex = *(int*)index;

	uint32_t retries = 0;
	int i;
	char key[KEYLEN];
	for(i = tindex*nobjs; i < (tindex+1)*nobjs; i++){
		memset(key, 0, KEYLEN);
		sprintf(key, "%d%d%d%d", i*4, i*2, i/2, i/4);
		if(cb_remove(key, &retries) == FAIL)
			verbose("Fail while removing object at thread #%d.", tindex);
	}
	verbose("Remove thread #%d retried %d times.", tindex, retries);
}

int main(int argc, char **argv){
	
	if(argc < 3){
		verbose("Usage: ./test #threads #objects");
		return;
	}

	nthreads = atoi(argv[1]);
	nobjs = atoi(argv[2]);
//	nobjs = 2000000;

	verbose("%d threads inserting %d objects each:", nthreads, nobjs);
	
//	Initialize data structure
	cb_init();

	pthread_t *threads;
	int *thread_index;

	threads = talloc(pthread_t, nthreads);
	thread_index = talloc(int, nthreads);

	struct timeval ti, tf;
	long int secs, msecs, usecs;
	long int isecs, imsecs, iusecs;
	long int fsecs, fmsecs, fusecs;
	long int rsecs, rmsecs, rusecs;

	int i;
/* 
 * INSERTING OBJECTS
 */
	gettimeofday(&ti, NULL);
	for(i = 0; i < nthreads; i++){
		thread_index[i] = i;
		if(pthread_create(&threads[i], NULL, &thread_insert, &thread_index[i]))
			error("Creation of insert thread #%d failed.", i);
	}
	verbose("Insert threads created successfully.");

	for(i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);
	gettimeofday(&tf, NULL);
	verbose("All insert threads are done.");

	uint64_t n_objs = cb_print(critbit->root);
	verbose("%ld object in the tree.", n_objs);

	time_calc();
	isecs = secs; imsecs = msecs; iusecs = usecs;

/*
 * FINDING OBJECTS
 */
	gettimeofday(&ti, NULL);
	for(i = 0; i < nthreads; i++){
		if(pthread_create(&threads[i], NULL, &thread_find, &thread_index[i]))
			error("Creation of find thread #%d failed.", i);
	}
	verbose("Find threads created successfully.");

	for(i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);
	gettimeofday(&tf, NULL);
	verbose("All find threads are done.");

	n_objs = cb_print(critbit->root);
	verbose("%ld objects in the tree.", n_objs);

	time_calc();
	fsecs = secs; fmsecs = msecs; fusecs = usecs;

/*
 * REMOVING OBJECT
 */
	gettimeofday(&ti, NULL);
	for(i = 0; i < nthreads; i++){
		if(pthread_create(&threads[i], NULL, &thread_remove, &thread_index[i]))
			error("Creation of remove thread #%d failed.", i);
	}
	verbose("Remove threads created successfully.");

	for(i = 0; i < nthreads; i++)
		pthread_join(threads[i], NULL);
	gettimeofday(&tf, NULL);
	verbose("All remove threads are done.");

	n_objs = cb_print(critbit->root);
	verbose("%ld objects in the tree.", n_objs);
	
	time_calc();
	rsecs = secs; rmsecs = msecs; rusecs = usecs;

	printf("\n");
	verbose("insertion-time: %ld.%ld%ld", isecs, imsecs, iusecs);
	verbose("search-time: %ld.%ld%ld", fsecs, fmsecs, fusecs);
	verbose("removing-time: %ld.%ld%ld", rsecs, rmsecs, rusecs);
}
