#include <assert.h>

#include "mpiv.h"
//#include "lc_priv.h"
#include "lc/pool.h"
#include "lc/affinity.h"
#include "lc/macro.h"
#include "thread.h"

static void* ctx_data;
static lc_pool* lc_req_pool;
void thread_yield();
void* thread_self();

LC_EXPORT
lch* lc_hdl;

static inline void MPI_Type_size(MPI_Datatype type, int *size)
{
  *size = (int) type;
}

void MPI_Recv(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag,
    MPI_Comm lc_hdl,
    MPI_Status* status __UNUSED__)
{
  int size = 1;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req ctx;
  lc_info info = {LC_SYNC_WAKE, LC_SYNC_WAKE, {.tag = tag}};
  while (!lc_recv_tag(lc_hdl, buffer, size, rank, &info, &ctx))
    thread_yield();
  lc_wait_post(thread_self(), &ctx);
}

void MPI_Send(void* buffer, int count, MPI_Datatype datatype,
    int rank, int tag, MPI_Comm lc_hdl __UNUSED__)
{
  int size = 1;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req ctx;
  lc_info info = {LC_SYNC_WAKE, LC_SYNC_WAKE, {.tag = tag}};
  while (!lc_send_tag(lc_hdl, buffer, size, rank, &info, &ctx))
    thread_yield();
  lc_wait_post(thread_self(), &ctx);
}

void MPI_Isend(const void* buf, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req *ctx = (lc_req*) lc_pool_get(lc_req_pool);
  lc_info info = {LC_SYNC_CNTR, LC_SYNC_CNTR, {.tag = tag}};
  while (!lc_send_tag(lc_hdl, buf, size, rank, &info, ctx))
    thread_yield();
  if (ctx->type != LC_REQ_DONE) {
    *req = (MPI_Request) ctx;
  } else {
    lc_pool_put(lc_req_pool, ctx);
    *req = MPI_REQUEST_NULL;
  }
}

void MPI_Irecv(void* buffer, int count, MPI_Datatype datatype, int rank,
                int tag, MPI_Comm lc_hdl __UNUSED__, MPI_Request* req) {
  int size;
  MPI_Type_size(datatype, &size);
  size *= count;
  lc_req *ctx = (lc_req*) lc_pool_get(lc_req_pool);
  lc_info info = {LC_SYNC_CNTR, LC_SYNC_CNTR, {.tag = tag}};
  while (!lc_recv_tag(lc_hdl, (void*) buffer, size, rank, &info, ctx))
    thread_yield();
  *req = (MPI_Request) ctx;
}

#if 1
void MPI_Waitall(int count, MPI_Request* req, MPI_Status* status __UNUSED__) {
  int pending = count;
  for (int i = 0; i < count; i++) {
    if (req[i] == MPI_REQUEST_NULL)
      pending--;
  }
  struct lc_cntr cntr = {thread_self(), pending};

  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_req* ctx = (lc_req *) req[i];
      if (lc_post(ctx, &cntr) == LC_OK) {
        __sync_fetch_and_sub(&cntr.count, 1);
        lc_pool_put(lc_req_pool, ctx);
        req[i] = MPI_REQUEST_NULL;
      }
    }
  }
  for (int i = 0; i < count; i++) {
    if (req[i] != MPI_REQUEST_NULL) {
      lc_req* ctx = (lc_req*) req[i];
      lc_sync_wait(cntr.sync, &ctx->int_type);
      lc_pool_put(lc_req_pool, ctx);
      req[i] = MPI_REQUEST_NULL;
    }
  }
}
#endif

volatile int lc_thread_stop;
static pthread_t progress_thread;

static void* progress(void* arg __UNUSED__)
{
  int c = 0;
  if (getenv("LC_POLL_CORE"))
    c = atoi(getenv("LC_POLL_CORE"));
  set_me_to(c);
  while (!lc_thread_stop) {
    while (lc_progress(lc_hdl))
      ;
  }
  return 0;
}

void MPI_Init(int* argc __UNUSED__, char*** args __UNUSED__)
{
  // setenv("LC_MPI", "1", 1);
  lc_open(&lc_hdl, 0);
  posix_memalign((void**) &ctx_data, 64, sizeof(lc_req) * 1024);
  lc_pool_create(&lc_req_pool);
  lc_req* ctxs = (lc_req*) ctx_data;
  for (int i = 0; i < 1024; i++)
    lc_pool_put(lc_req_pool, &ctxs[i]);
  lc_thread_stop = 0;
  pthread_create(&progress_thread, 0, progress, 0);
}

void MPI_Finalize()
{
  lc_thread_stop = 1;
  pthread_join(progress_thread, 0);
  free(ctx_data);
  lc_close(lc_hdl);
}

void MPI_Barrier(MPI_Comm mv)
{
  lc_barrier(mv);
}
