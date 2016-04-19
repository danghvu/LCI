#ifndef MPIV_ABT_INL_H_
#define MPIV_ABT_INL_H_

#include "affinity.h"

__thread abt_thread* __fulting = NULL;

abt_thread::abt_thread() {
  ABT_mutex_create(&mutex_);
  ABT_cond_create(&cond_);
}

void abt_thread::yield() {
  ABT_thread_yield();
  __fulting = this;
}

void abt_thread::wait(bool& flag) {
  assert(__fulting);
  ABT_mutex_lock(mutex_);
  while (!flag) {
    ABT_cond_wait(cond_, mutex_);
  }
  ABT_mutex_unlock(mutex_);
  __fulting = this;
}

void abt_thread::resume(bool& flag) {
  ABT_mutex_lock(mutex_);
  flag = true;
  ABT_mutex_unlock(mutex_);
  ABT_cond_signal(cond_);
}

void abt_thread::join() {
  ABT_thread_join(th_);
  __fulting = this;
}

int abt_thread::get_worker_id() {
  return origin_->id();
}

static void abt_fwrapper(void* arg) {
  abt_thread* th = (abt_thread*) arg;
  __fulting = th;
  th->f(th->data);
  __fulting = NULL;
}

abt_thread* abt_worker::spawn(ffunc f, intptr_t data, size_t stack_size) {
  abt_thread *th = new abt_thread();
  th->f = f;
  th->data = data;
  th->origin_ = this;
  ABT_thread_attr_create(&(th->attr_));
  ABT_thread_attr_set_stacksize(th->attr_, stack_size);
  ABT_thread_create(
      pool_, abt_fwrapper, (void*) th, th->attr_,
      &(th->th_));
  return th;
}

std::atomic<int> abt_nworker;

static void abt_start_up(void* arg) {
  long id = (long) arg;
  affinity::set_me_to(id);
}

void abt_worker::start() {
  id_ = abt_nworker.fetch_add(1);
  ABT_xstream_create(ABT_SCHED_NULL, &xstream_);
  ABT_xstream_get_main_pools(xstream_, 1, &pool_);
  ABT_xstream_start(xstream_);

  ABT_thread start_up;
  ABT_thread_create(
      pool_, abt_start_up, (void*) (long) id_, ABT_THREAD_ATTR_NULL,
      &start_up);
  ABT_thread_join(start_up);
}

void abt_worker::stop() {
  ABT_xstream_join(xstream_);
  ABT_xstream_free(&xstream_);
}

void abt_worker::start_main(ffunc main_task, intptr_t data) {
  id_ = abt_nworker.fetch_add(1);
  affinity::set_me_to(id_);
  ABT_xstream_self(&xstream_);
  ABT_xstream_get_main_pools(xstream_, 1, &pool_);
  ABT_xstream_start(xstream_);
  abt_thread* t = spawn(main_task, data, MAIN_STACK_SIZE);
  t->join();
  delete t;
}

void abt_worker::stop_main() {
}

#endif