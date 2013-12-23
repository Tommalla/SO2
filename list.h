/* Tomasz Zakrzewski, nr indeksu: 336079 /
/  SO2013 - second task			*/
#ifndef LIST_H
#define LIST_H
#include <pthread.h>
#include <stdlib.h>


struct Node {
	struct Node* prev;
	pthread_t th;
	struct Node* next;
};


static struct Node* list = NULL;


void prepend(const pthread_t th, pthread_mutex_t* const mutex) {
	struct Node* v = (struct Node*) malloc(sizeof(struct Node));
	pthread_mutex_lock(mutex);
	v->prev = NULL;
	v->next = list;
	if (list != NULL)
		list->prev = v;
	list = v;
	pthread_mutex_unlock(mutex);
}


void deleteNode(struct Node* const v, pthread_mutex_t* const mutex) {
	if (v == NULL)
		return;
	pthread_mutex_lock(mutex);
	struct Node* p = v->prev;
	struct Node* n = v->next;

	if (p != NULL)
		p->next = n;
	if (n != NULL)
		n->prev = p;

	if (v == list)	//it was the first
		list = n;

	free(v);
	pthread_mutex_unlock(mutex);
}


#endif