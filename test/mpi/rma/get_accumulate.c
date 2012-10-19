/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *
 *  (C) 2012 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "mpitest.h"

/* MPI-3 is not yet standardized -- allow MPI-3 routines to be switched off.
 */

#if !defined(USE_STRICT_MPI) && defined(MPICH2)
#  define TEST_MPI3_ROUTINES 1
#endif

static const int SQ_LIMIT = 10;
static       int SQ_COUNT = 0;

#define SQUELCH(X)                              \
  do {                                          \
    if (SQ_COUNT < SQ_LIMIT || verbose) {       \
      SQ_COUNT++;                               \
      X                                         \
    }                                           \
  } while (0)

#define ITER  100
#define COUNT 5

#if defined (GACC_TYPE_SHORT)
#  define TYPE_C   short
#  define TYPE_MPI_BASE MPI_SHORT
#  define TYPE_FMT "%d"
#elif defined (GACC_TYPE_LONG)
#  define TYPE_C   long
#  define TYPE_MPI_BASE MPI_LONG
#  define TYPE_FMT "%ld"
#elif defined (GACC_TYPE_DOUBLE)
#  define TYPE_C   double
#  define TYPE_MPI_BASE MPI_DOUBLE
#  define TYPE_FMT "%f"
#else
#  define TYPE_C   int
#  define TYPE_MPI_BASE MPI_INT
#  define TYPE_FMT "%d"
#endif

#if defined(GACC_TYPE_DERIVED)
#  define TYPE_MPI derived_type
#else
#  define TYPE_MPI TYPE_MPI_BASE
#endif

const int verbose = 0;

void reset_bufs(TYPE_C *win_ptr, TYPE_C *res_ptr, TYPE_C *val_ptr, TYPE_C value, MPI_Win win) {
    int rank, nproc, i;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    memset(win_ptr, 0, sizeof(TYPE_C)*nproc*COUNT);
    MPI_Win_unlock(rank, win);

    memset(res_ptr, -1, sizeof(TYPE_C)*nproc*COUNT);

    for (i = 0; i < COUNT; i++)
        val_ptr[i] = value;

    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char **argv) {
    int       i, rank, nproc;
    int       errors = 0, all_errors = 0;
    TYPE_C   *win_ptr, *res_ptr, *val_ptr;
    MPI_Win   win;
#if defined (GACC_TYPE_DERIVED)
    MPI_Datatype derived_type;
#endif

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    win_ptr = malloc(sizeof(TYPE_C)*nproc*COUNT);
    res_ptr = malloc(sizeof(TYPE_C)*nproc*COUNT);
    val_ptr = malloc(sizeof(TYPE_C)*COUNT);

#if defined (GACC_TYPE_DERIVED)
    MPI_Type_contiguous(1, TYPE_MPI_BASE, &derived_type);
    MPI_Type_commit(&derived_type);
#endif

    MPI_Win_create(win_ptr, sizeof(TYPE_C)*nproc*COUNT, sizeof(TYPE_C),
                   MPI_INFO_NULL, MPI_COMM_WORLD, &win);

#ifdef TEST_MPI3_ROUTINES

    /* Test self communication */

    reset_bufs(win_ptr, res_ptr, val_ptr, 1, win);

    for (i = 0; i < ITER; i++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
        MPI_Get_accumulate(val_ptr, COUNT, TYPE_MPI, res_ptr, COUNT, TYPE_MPI, 
                            rank, 0, COUNT, TYPE_MPI, MPI_SUM, win);
        MPI_Win_unlock(rank, win);
    }

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < COUNT; i++) {
        if (win_ptr[i] != ITER) {
            SQUELCH( printf("%d->%d -- SELF[%d]: expected "TYPE_FMT", got "TYPE_FMT"\n",
                            rank, rank, i, (TYPE_C) ITER, win_ptr[i]); );
            errors++;
        }
    }
    MPI_Win_unlock(rank, win);

    /* Test neighbor communication */

    reset_bufs(win_ptr, res_ptr, val_ptr, 1, win);

    for (i = 0; i < ITER; i++) {
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, (rank+1)%nproc, 0, win);
        MPI_Get_accumulate(val_ptr, COUNT, TYPE_MPI, res_ptr, COUNT, TYPE_MPI, 
                            (rank+1)%nproc, 0, COUNT, TYPE_MPI, MPI_SUM, win);
        MPI_Win_unlock((rank+1)%nproc, win);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < COUNT; i++) {
        if (win_ptr[i] != ITER) {
            SQUELCH( printf("%d->%d -- NEIGHBOR[%d]: expected "TYPE_FMT", got "TYPE_FMT"\n",
                            (rank+1)%nproc, rank, i, (TYPE_C) ITER, win_ptr[i]); );
            errors++;
        }
    }
    MPI_Win_unlock(rank, win);

    /* Test contention */

    reset_bufs(win_ptr, res_ptr, val_ptr, 1, win);

    if (rank != 0) {
        for (i = 0; i < ITER; i++) {
            MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
            MPI_Get_accumulate(val_ptr, COUNT, TYPE_MPI, res_ptr, COUNT, TYPE_MPI, 
                                0, 0, COUNT, TYPE_MPI, MPI_SUM, win);
            MPI_Win_unlock(0, win);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    if (rank == 0 && nproc > 1) {
        for (i = 0; i < COUNT; i++) {
            if (win_ptr[i] != ITER*(nproc-1)) {
                SQUELCH( printf("*->%d - CONTENTION[%d]: expected="TYPE_FMT" val="TYPE_FMT"\n",
                                rank, i, (TYPE_C) ITER*(nproc-1), win_ptr[i]); );
                errors++;
            }
        }
    }
    MPI_Win_unlock(rank, win);

    /* Test all-to-all communication */

    reset_bufs(win_ptr, res_ptr, val_ptr, rank, win);

    for (i = 0; i < ITER; i++) {
        int j;

        MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
        for (j = 0; j < nproc; j++) {
            MPI_Get_accumulate(val_ptr, COUNT, TYPE_MPI, &res_ptr[j*COUNT], COUNT, TYPE_MPI,
                                j, rank*COUNT, COUNT, TYPE_MPI, MPI_SUM, win);
        }
        MPI_Win_fence(MPI_MODE_NOSUCCEED, win);
        MPI_Barrier(MPI_COMM_WORLD);

        for (j = 0; j < nproc; j++) {
            int c;
            for (c = 0; c < COUNT; c++) {
                if (res_ptr[j*COUNT+c] != i*rank) {
                    SQUELCH( printf("%d->%d -- ALL-TO-ALL[%d]: iter %d, expected result "TYPE_FMT", got "TYPE_FMT"\n", 
                                    rank, j, c, i, (TYPE_C) i*rank, res_ptr[j*COUNT+c]); );
                    errors++;
                }
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, win);
    for (i = 0; i < nproc; i++) {
        int c;
        for (c = 0; c < COUNT; c++) {
            if (win_ptr[i*COUNT+c] != ITER*i) {
                SQUELCH( printf("%d->%d -- ALL-TO-ALL: expected "TYPE_FMT", got "TYPE_FMT"\n", 
                                i, rank, (TYPE_C) ITER*i, win_ptr[i*COUNT+c]); );
                errors++;
            }
        }
    }
    MPI_Win_unlock(rank, win);

#endif /* TEST_MPI3_ROUTINES */

    MPI_Win_free(&win);

    MPI_Reduce(&errors, &all_errors, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0 && all_errors == 0)
        printf(" No Errors\n");

#if defined (GACC_TYPE_DERIVED)
    MPI_Type_free(&derived_type);
#endif

    free(win_ptr);
    free(res_ptr);
    free(val_ptr);

    MPI_Finalize();

    return 0;
}
