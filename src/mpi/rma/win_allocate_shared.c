/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpiimpl.h"
#include "rma.h"

/* -- Begin Profiling Symbol Block for routine MPI_Win_allocate_shared */
#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_Win_allocate_shared = PMPI_Win_allocate_shared
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_Win_allocate_shared  MPI_Win_allocate_shared
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_Win_allocate_shared as PMPI_Win_allocate_shared
#endif
/* -- End Profiling Symbol Block */

/* Define MPICH_MPI_FROM_PMPI if weak symbols are not supported to build
   the MPI routines */
#ifndef MPICH_MPI_FROM_PMPI
#undef MPI_Win_allocate_shared
#define MPI_Win_allocate_shared PMPI_Win_allocate_shared

#endif

#undef FUNCNAME
#define FUNCNAME MPI_Win_allocate_shared
#undef FCNAME
#define FCNAME MPIU_QUOTE(FUNCNAME)

/*@
   MPI_Win_allocate_shared - Create an MPI Window object for one-sided communication
   and allocate memory at each process.

   Input Parameters:
. size - size of window in bytes (nonnegative integer)
. info - info argument (handle)
- comm - communicator (handle)

  Output Parameter:
. baseptr - initial address of window (choice)
- win - window object returned by the call (handle)

.N ThreadSafe
.N Fortran

.N Errors
.N MPI_SUCCESS
.N MPI_ERR_ARG
.N MPI_ERR_COMM
.N MPI_ERR_INFO
.N MPI_ERR_OTHER
.N MPI_ERR_SIZE
@*/
int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                             void *baseptr, MPI_Win *win)
{
    int mpi_errno = MPI_SUCCESS;
    MPID_Win *win_ptr = NULL;
    MPID_Comm *comm_ptr = NULL;
    MPID_Info *info_ptr = NULL;
    MPID_MPI_STATE_DECL(MPID_STATE_MPI_WIN_ALLOCATE_SHARED);

    MPIR_ERRTEST_INITIALIZED_ORDIE();

    MPIU_THREAD_CS_ENTER(ALLFUNC,);
    MPID_MPI_RMA_FUNC_ENTER(MPID_STATE_MPI_WIN_ALLOCATE_SHARED);

    /* Validate parameters, especially handles needing to be converted */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
	    MPIR_ERRTEST_COMM(comm, mpi_errno);
            MPIR_ERRTEST_INFO_OR_NULL(info, mpi_errno);
            MPIR_ERRTEST_ARGNULL(win, "win", mpi_errno);
	}
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* Convert MPI object handles to object pointers */
    MPID_Comm_get_ptr( comm, comm_ptr );
    MPID_Info_get_ptr( info, info_ptr );

    /* Validate parameters and objects (post conversion) */
#   ifdef HAVE_ERROR_CHECKING
    {
        MPID_BEGIN_ERROR_CHECKS;
        {
            /* Validate pointers */
	    MPID_Comm_valid_ptr( comm_ptr, mpi_errno );
            if (mpi_errno != MPI_SUCCESS) goto fn_fail;

            MPIU_ERR_CHKANDJUMP1(disp_unit <= 0, mpi_errno, MPI_ERR_ARG,
                                 "**arg", "**arg %s", "disp_unit must be positive");

            MPIU_ERR_CHKANDJUMP1(size < 0, mpi_errno, MPI_ERR_SIZE,
                                 "**rmasize", "**rmasize %d", size);

            MPIU_ERR_CHKANDJUMP1(size > 0 && baseptr == NULL, mpi_errno, MPI_ERR_ARG,
                                 "**nullptr", "**nullptr %s",
                                 "NULL base pointer is invalid when size is nonzero");

            if (mpi_errno != MPI_SUCCESS) goto fn_fail;
        }
        MPID_END_ERROR_CHECKS;
    }
#   endif /* HAVE_ERROR_CHECKING */

    /* ... body of routine ...  */

    mpi_errno = MPID_Win_allocate_shared(size, disp_unit, info_ptr, comm_ptr, baseptr, &win_ptr);
    if (mpi_errno != MPI_SUCCESS) goto fn_fail;

    /* Initialize a few fields that have specific defaults */
    win_ptr->name[0]    = 0;
    win_ptr->errhandler = 0;
    win_ptr->lockRank   = MPID_WIN_STATE_UNLOCKED;

    /* return the handle of the window object to the user */
    MPIU_OBJ_PUBLISH_HANDLE(*win, win_ptr->handle);

    /* ... end of body of routine ... */

  fn_exit:
    MPID_MPI_RMA_FUNC_EXIT(MPID_STATE_MPI_WIN_ALLOCATE_SHARED);
    MPIU_THREAD_CS_EXIT(ALLFUNC,);
    return mpi_errno;

  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
#   ifdef HAVE_ERROR_CHECKING
    {
	mpi_errno = MPIR_Err_create_code(
	    mpi_errno, MPIR_ERR_RECOVERABLE, FCNAME, __LINE__, MPI_ERR_OTHER, "**mpi_win_allocate_shared",
	    "**mpi_win_allocate_shared %d %I %C %p %p", size, info, comm, baseptr, win);
    }
#   endif
    mpi_errno = MPIR_Err_return_comm( comm_ptr, FCNAME, mpi_errno );
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}
