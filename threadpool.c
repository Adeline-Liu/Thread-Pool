#include <stdlib.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"
#include "threadpool.h"

struct tpool {
    // TODO
  uthread_mutex_t mutex;
  uthread_cond_t cond;

  int max_threads;

  uthread_t *threads;

  int canEnd;
  int isRunning;

  struct task *nextTask;
  struct task *lastTask;

  // int start; don't need?
};

struct task {
  void (*f)(tpool_t, void*);
  void *arg;
  struct task *next;
  
  // struct task *last;
};

/**
 * Base procedure of every worker thread.  Calls available tasks
 * or blocks until a task becomes available.
 */
void *worker_thread(void *pool_v) {
  tpool_t pool = pool_v;

  uthread_mutex_lock(pool->mutex);
      // still has task to do 
  while ((pool->nextTask) || (pool->isRunning) || (!pool->canEnd)) {

      // doesn't have task to do
    if (!pool->nextTask) {
      uthread_cond_wait(pool->cond);
    }
    
    struct task *task = pool->nextTask;

    if (task) {
      pool->nextTask = task->next;

      if (!task->next) {
        pool->lastTask = NULL;
      }

      pool->isRunning++;

      uthread_mutex_unlock(pool->mutex);
      task->f(pool, task->arg);

      free(task);

      uthread_mutex_lock(pool->mutex);
      pool->isRunning--;

      if ((pool->canEnd) && (!pool->isRunning)) {
        uthread_cond_broadcast(pool->cond);
      }
    }
  }
  uthread_mutex_unlock(pool->mutex);

  return pool; // TODO: replace this placehold return value
}

/**
 * Create a new thread pool with max_threads thread-count limit.
 */
tpool_t tpool_create(unsigned int max_threads) {
    // TODO
    tpool_t pool = malloc(sizeof(struct tpool));
    
    // create the tpool
    pool->max_threads = max_threads;
    pool->threads = malloc(pool->max_threads * sizeof(uthread_t));

    pool->mutex = uthread_mutex_create();
    pool->cond = uthread_cond_create(pool->mutex);
  
    // threads inside
    for (int i = 0; i < pool->max_threads; i++)
      pool->threads[i] = uthread_create(worker_thread, pool);

    return pool;
}

/**
 * Sechedule task f(arg) to be executed.
 */
void tpool_schedule_task(tpool_t pool, void (*f)(tpool_t, void *), void *arg) {
    // TODO

  uthread_mutex_lock(pool->mutex);
  
  // create the task
  struct task *task = malloc(sizeof(struct task));
  task->f = f;
  task->arg = arg;
  task->next = NULL;

  // last task isn't null, schedule this task
  if (pool->lastTask) {
    pool->lastTask->next = task;
  }

  pool->lastTask = task;

  // next task is null, schedule this task
  if (!pool->nextTask) {
    pool->nextTask = task;
  }

  uthread_cond_signal(pool->cond);
  uthread_mutex_unlock(pool->mutex);
}

/**
 * Wait (by blocking) until all tasks have completed and thread pool is thus idle
 */
void tpool_join(tpool_t pool) {
    // TODO
  uthread_mutex_lock(pool->mutex);
  pool->canEnd = 1;

  uthread_cond_broadcast(pool->cond);
  uthread_mutex_unlock(pool->mutex);

  for (int i = 0; i < pool->max_threads; i++) {
    uthread_join(pool->threads[i], 0);
  }
  
  uthread_cond_destroy(pool->cond);
  uthread_mutex_destroy(pool->mutex);

  free(pool->threads);
  free(pool);
}
