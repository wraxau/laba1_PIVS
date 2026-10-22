/* Односвязный список на основе выданного pth_ll_rwl.c. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include "my_rand.h"

/* Один код списка собирается с двумя разными блокировками */
#ifdef USE_CUSTOM
#include "my_rwlock.h"
my_rwlock_t rwlock;
#define INIT() my_rwlock_init(&rwlock)
#define RDLOCK() my_rwlock_rdlock(&rwlock)
#define WRLOCK() my_rwlock_wrlock(&rwlock)
#define UNLOCK() my_rwlock_unlock(&rwlock)
#define DESTROY() my_rwlock_destroy(&rwlock)
#else
pthread_rwlock_t rwlock;
#define INIT() pthread_rwlock_init(&rwlock, NULL)
#define RDLOCK() pthread_rwlock_rdlock(&rwlock)
#define WRLOCK() pthread_rwlock_wrlock (&rwlock)
#define UNLOCK() pthread_rwlock_unlock (&rwlock)
#define DESTROY() pthread_rwlock_destroy (&rwlock)
#endif

#define MAX_KEY 100000000

struct list_node_s {
    int data;
    struct list_node_s *next;
};

struct list_node_s *head = NULL;
int thread_count, total_ops;
double search_percent, insert_percent;
int member_count = 0, insert_count = 0, delete_count = 0;
int found_count = 0;
pthread_mutex_t count_mutex;

int Member(int value) {
    struct list_node_s *curr = head;
    while (curr != NULL && curr->data < value)
        curr = curr->next;
    return curr != NULL && curr->data == value;
}

int Insert(int value) {
    struct list_node_s *curr = head;
    struct list_node_s *pred = NULL;
    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }
    if (curr!=NULL && curr->data==value)
        return 0;

    struct list_node_s *node = malloc(sizeof(struct list_node_s));
    node->data =value;
    node->next =curr;
    if (pred==NULL)
        head =node;
    else
        pred->next=node;
    return 1;
}

int Delete(int value) {
    struct list_node_s *curr = head;
    struct list_node_s *pred = NULL;
    while (curr != NULL && curr->data < value) {
        pred = curr;
        curr = curr->next;
    }
    if (curr==NULL||curr->data!=value)
        return 0;

    if (pred==NULL)
        head=curr->next;
    else
        pred->next=curr->next;
    free(curr);
    return 1;
}

void Free_list(void) {
    while (head != NULL) {
        struct list_node_s *temp = head;
        head=head->next;
        free(temp);
    }
}

double Get_time(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec+t.tv_nsec /1e9;
}

void *Thread_work(void *rank) {
    int my_rank=(int)(intptr_t)rank;
    unsigned seed = my_rank +1;
    int searches =0, inserts= 0, deletes= 0, found=0;
    int count = total_ops / thread_count;
    if (my_rank < total_ops % thread_count)
        count++;

    for (int i=0; i<count; i++) {
        double operation = my_drand(&seed);
        int value =my_rand(&seed) % MAX_KEY;
        if (operation< search_percent) {
            RDLOCK();
            found += Member(value);
            UNLOCK();
            searches++;
        } else if (operation < search_percent + insert_percent) {
            WRLOCK();
            Insert(value);
            UNLOCK();
            inserts++;
        } else {
            WRLOCK();
            Delete(value);
            UNLOCK();
            deletes++;
        }
    }

    pthread_mutex_lock(&count_mutex);
    member_count+=searches;
    insert_count+=inserts;
    delete_count += deletes;
    found_count += found;
    pthread_mutex_unlock(&count_mutex);
    return NULL;
}

int main(int argc,char *argv[]) {
    if (argc!=2) {
        printf("Usage: %s thread_count\n", argv[0]);
        return 1;
    }
    thread_count =atoi(argv[1]);
    if (thread_count<1)
        return 1;

    int initial_count;
    printf("Initial list size, total operations, search share, insert share:\n");
    if (scanf("%d %d %lf %lf", &initial_count, &total_ops,
              &search_percent, &insert_percent) != 4)
        return 1;

    unsigned seed=1;
    int inserted=0;
    for (int attempts=0; inserted < initial_count && attempts < 2 * initial_count; attempts++)
        inserted+=Insert(my_rand(&seed) % MAX_KEY);
    printf("Inserted %d keys\n", inserted);

    pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
    INIT();
    pthread_mutex_init(&count_mutex, NULL);
    double start=Get_time();

    for (int i = 0;i< thread_count; i++)
        pthread_create(&threads[i], NULL, Thread_work, (void *)(intptr_t)i);
    for (int i = 0;i< thread_count; i++)
        pthread_join(threads[i], NULL);

    double finish = Get_time();
    printf("Elapsed time = %e seconds\n", finish - start);
    printf("member ops = %d\ninsert ops = %d\ndelete ops = %d\n",
           member_count, insert_count, delete_count);
    printf("Found = %d\n", found_count);

    DESTROY();
    pthread_mutex_destroy(&count_mutex);
    free(threads);
    Free_list();
    return 0;
}
