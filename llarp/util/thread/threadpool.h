#ifndef LLARP_THREADPOOL_H
#define LLARP_THREADPOOL_H

#include <util/string_view.hpp>
#include <util/thread/queue.hpp>
#include <util/thread/threading.hpp>
#include <util/types.hpp>

#include <absl/base/thread_annotations.h>
#include <memory>
#include <queue>

struct llarp_threadpool;

#ifdef __cplusplus
struct llarp_threadpool
{
  struct Impl;
  Impl *impl;

  llarp_threadpool(int workers, llarp::string_view name,
                   size_t queueLength = size_t{1024 * 8});

  ~llarp_threadpool();

  size_t
  size() const;

  size_t
  pendingJobs() const;

  size_t
  numThreads() const;

  /// try to guess how big our job latency is on this threadpool
  llarp_time_t
  GuessJobLatency(llarp_time_t granulairty = 1000) const;

  /// see if this thread is full given lookahead amount
  bool
  LooksFull(size_t lookahead) const
  {
    return (pendingJobs() + lookahead) >= size();
  }
};
#endif

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name);

void
llarp_free_threadpool(struct llarp_threadpool **tp);

using llarp_thread_work_func = void (*)(void *);

/** job to be done in worker thread */
struct llarp_thread_job
{
#ifdef __cplusplus
  /** user data to pass to work function */
  void *user{nullptr};
  /** called in threadpool worker thread */
  llarp_thread_work_func work{nullptr};

  llarp_thread_job(void *u, llarp_thread_work_func w) : user(u), work(w)
  {
  }

  struct ContextWrapper
  {
    std::function< void(void) > func;

    ContextWrapper(std::function< void(void) > f) : func(f)
    {
    }

    static void
    Work(void *user)
    {
      ContextWrapper *u = static_cast< ContextWrapper * >(user);
      u->func();
      delete u;
    }
  };

  llarp_thread_job(std::function< void(void) > f)
  {
    user = new llarp_thread_job::ContextWrapper(f);
    work = &llarp_thread_job::ContextWrapper::Work;
  }

  llarp_thread_job() = default;

  void
  operator()() const
  {
    work(user);
  }

#else
  void *user;
  llarp_thread_work_func work;
#endif
};

void
llarp_threadpool_tick(struct llarp_threadpool *tp);

bool
llarp_threadpool_queue_job(struct llarp_threadpool *tp,
                           struct llarp_thread_job j);

#ifdef __cplusplus

bool
llarp_threadpool_queue_job(struct llarp_threadpool *tp,
                           std::function< void(void) > func);

#endif

void
llarp_threadpool_start(struct llarp_threadpool *tp);
void
llarp_threadpool_stop(struct llarp_threadpool *tp);

#include <util/thread/thread_pool.hpp>

#endif
