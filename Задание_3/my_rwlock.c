#include "my_rwlock.h"

void my_rwlock_init(my_rwlock_t *lock) {
    pthread_mutex_init(&lock->mutex,NULL);
    pthread_cond_init(&lock->readers,NULL);
    pthread_cond_init(&lock->writers,NULL);
    lock->active_readers = 0;
    lock->waiting_readers = 0;
    lock->waiting_writers = 0;
    lock->active_writer = 0;
}

void my_rwlock_rdlock (my_rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->waiting_readers++;

    /* Новые читатели пропускают ожидающего писателя */
    while (lock->active_writer || lock->waiting_writers>0)
        pthread_cond_wait(&lock->readers, &lock->mutex);

    lock->waiting_readers--;
    lock->active_readers++;
    pthread_mutex_unlock(&lock->mutex);
}

void my_rwlock_wrlock (my_rwlock_t *lock) {
    pthread_mutex_lock (&lock->mutex);
    lock->waiting_writers++;

    while ( lock->active_writer || lock->active_readers > 0)
        pthread_cond_wait(&lock->writers, &lock->mutex);

    lock -> waiting_writers--;
    lock -> active_writer =1;
    pthread_mutex_unlock(&lock->mutex);
}

void my_rwlock_unlock(my_rwlock_t *lock) {
    pthread_mutex_lock (&lock->mutex) ;

    if (lock->active_writer)
        lock->active_writer = 0;
    else
        lock->active_readers--;

    /* После последнего владельца будим писателя или всех читателей */
    if (!lock->active_writer && lock->active_readers == 0) {
        if (lock->waiting_writers > 0)
            pthread_cond_signal(&lock->writers);
        else if (lock->waiting_readers > 0)
            pthread_cond_broadcast(&lock->readers);
    }

    pthread_mutex_unlock(&lock->mutex);
}

void my_rwlock_destroy(my_rwlock_t *lock) {
    pthread_mutex_destroy (&lock->mutex) ;
    pthread_cond_destroy (&lock->readers);
    pthread_cond_destroy (&lock->writers);
}
