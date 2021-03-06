#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lc.h"
#include "comm_exp.h"

lch* mv;
int WINDOWS = 1;

int main(int argc, char** args)
{
  lc_open(&mv, 1);

  if (argc > 1)
    WINDOWS = atoi(args[1]);

  int rank = lc_id(mv);
  int total, skip;
  void* buffer, *rx_buffer;
  posix_memalign(&buffer, 4096, 1<<22);
  posix_memalign(&rx_buffer, 4096, 1<<22);
  lc_req ctx;
  for (size_t len = 1; len <= (1 << 22); len <<= 1) {
    if (len > 8192) {
      skip = SKIP_LARGE;
      total = TOTAL_LARGE;
    } else {
      skip = SKIP;
      total = TOTAL;
    }
    struct lc_pkt pkt;
    lc_pkt_init(mv, len, &pkt);
    memset(pkt.buffer, 'A', len);
    if (rank == 0) {
      double t1;
      for (int i = 0; i < skip + total; i++) {
        if (i == skip) t1 = wtime();
        for (int j = 0; j < WINDOWS; j++)  {
          lc_send_queue_p(mv, &pkt, 1, 0, 0, &ctx);
          lc_wait_poll(mv, &ctx);
        }
        int rank; lc_qtag tag;
        size_t size;
        while (!lc_recv_queue_probe(mv, &size, &rank, &tag, 0, &ctx)) {
          lc_progress(mv);
        }
        lc_recv_queue(mv, rx_buffer, &ctx);
        lc_wait_poll(mv, &ctx);
      }
      printf("%llu \t %.5f\n", len, (wtime() - t1)/total / (WINDOWS+1) * 1e6);
    } else {
      for (int i = 0; i < skip + total; i++) {
        int rank; lc_qtag tag;
        size_t size;
        for (int j = 0; j < WINDOWS; j++) {
          while (!lc_recv_queue_probe(mv, &size, &rank, &tag, 0, &ctx)) {
            lc_progress(mv);
          }
          lc_recv_queue(mv, rx_buffer, &ctx);
          lc_wait_poll(mv, &ctx);
        }
        lc_send_queue_p(mv, &pkt, 0, i, 0, &ctx);
        lc_wait_poll(mv, &ctx);
      }
    }
  }
  free(rx_buffer);
  lc_close(mv);
  return 0;
}
