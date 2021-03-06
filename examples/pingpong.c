#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "mpiv.h"
#include "comm_exp.h"

#ifdef USE_ABT
#include "helper_abt.h"
#elif defined(USE_PTH)
#include "helper_pth.h"
#else
#include "helper_fult.h"
#endif

#define CHECK_RESULT 0

#define MIN_MSG_SIZE 1
#define MAX_MSG_SIZE (1 << 22)

int size = 0;

int main(int argc, char** args)
{
  setenv("LC_POLL_CORE", "15", 1);
  MPI_Init(&argc, &args);
  if (argc > 1) size = atoi(args[1]);
  MPI_Start_worker(1);
  set_me_to(14);

  double times = 0;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  void* r_buf = 0, *s_buf = 0;
  posix_memalign((void**) &r_buf, 4096, (size_t)MAX_MSG_SIZE);
  posix_memalign((void**) &s_buf, 4096, (size_t)MAX_MSG_SIZE);

  for (int size = MIN_MSG_SIZE; size <= MAX_MSG_SIZE; size <<= 1) {
    int total = TOTAL;
    int skip = SKIP;

    if (size > LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    // MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      memset(r_buf, 'a', size);
      memset(s_buf, 'b', size);
      for (int t = 0; t < total + skip; t++) {
        if (t == skip) {
          times = wtime();
        }
        MPI_Send(s_buf, size, MPI_CHAR, 1, 1, MPI_COMM_WORLD);
        MPI_Recv(r_buf, size, MPI_CHAR, 1, 2, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        if (t == 0 || CHECK_RESULT) {
          for (int j = 0; j < size; j++) {
            assert(((char*)r_buf)[j] == 'b');
            assert(((char*)s_buf)[j] == 'b');
          }
        }
      }
      times = wtime() - times;
      printf("[%d] %f\n", size, times * 1e6 / total / 2);//, (mv_ptime - ptime) * 1e6/ total / 2);
    } else {
      memset(s_buf, 'b', size);
      memset(r_buf, 'a', size);
      for (int t = 0; t < total + skip; t++) {
        MPI_Recv(r_buf, size, MPI_CHAR, 0, 1, MPI_COMM_WORLD,
                  MPI_STATUS_IGNORE);
        MPI_Send(s_buf, size, MPI_CHAR, 0, 2, MPI_COMM_WORLD);
      }
      // printf("[%d] %f\n", size, rx_time * 1e6 / total / 2);//, (mv_ptime - ptime) * 1e6/ total / 2);
    }
    // MPI_Barrier(MPI_COMM_WORLD);
  }

  free(r_buf);
  free(s_buf);

  MPI_Stop_worker();
  MPI_Finalize();
}
