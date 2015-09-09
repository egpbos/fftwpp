/*
 * Copyright (c) 2003, 2007-11 Matteo Frigo
 * Copyright (c) 2003, 2007-11 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <mpi.h>
#include <fftw3.h>
#include <cstdlib>
#include <cstring>

namespace fftwpp {

/* Import wisdom from all processes to process 0, as prelude to
   exporting a single wisdom file (this is convenient when we are
   running on identical processors, to avoid the annoyance of having
   per-process wisdom files).  In order to make the time for this
   operation logarithmic in the number of processors (rather than
   linear), we employ a tree reduction algorithm.  This means that the
   wisdom is modified on processes other than root, which shouldn't
   matter in practice. */
void mpi_gather_wisdom(const MPI_Comm& comm_)
{
  MPI_Comm comm, comm2;
  int my_pe, n_pes;
  char *wis;
  size_t wislen;
  MPI_Status status;

  MPI_Comm_dup(comm_, &comm);
  MPI_Comm_rank(comm, &my_pe);
  MPI_Comm_size(comm, &n_pes);

  if (n_pes > 2) { /* recursively split into even/odd processes */
    MPI_Comm_split(comm, my_pe % 2, my_pe, &comm2);
    mpi_gather_wisdom(comm2);
    MPI_Comm_free(&comm2);
  }
  if (n_pes > 1 && my_pe < 2) { /* import process 1 -> 0 */
    if (my_pe == 1) {
      wis = fftw_export_wisdom_to_string();
      wislen = strlen(wis) + 1;
      MPI_Send(&wislen, 1, MPI_UNSIGNED_LONG_LONG, 0, 111, comm);
      MPI_Send(wis, wislen, MPI_CHAR, 0, 222, comm);
      fftw_free(wis);
    }
    else /* my_pe == 0 */ {
      MPI_Recv(&wislen, 1, MPI_UNSIGNED_LONG_LONG, 1, 111, comm, &status);
      char wis[wislen];
      MPI_Recv(wis, wislen, MPI_CHAR, 1, 222, comm, &status);
      if (!fftw_import_wisdom_from_string(wis))
        MPI_Abort(comm, 1);
    }
  }
  MPI_Comm_free(&comm);
}

/* broadcast wisdom from process 0 to all other processes; this
   is useful so that we can import wisdom once and not worry
   about parallel I/O or process-specific wisdom, although of
   course it assumes that all the processes have identical
   performance characteristics (i.e. identical hardware). */
void mpi_broadcast_wisdom(const MPI_Comm& comm_)
{
  MPI_Comm comm;
  int my_pe;
  char *wis;
  size_t wislen;

  MPI_Comm_dup(comm_, &comm);
  MPI_Comm_rank(comm, &my_pe);

  if (my_pe != 0) {
    MPI_Bcast(&wislen, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
    char wis[wislen];
    MPI_Bcast(wis, wislen, MPI_CHAR, 0, comm);
    if (!fftw_import_wisdom_from_string(wis))
      MPI_Abort(comm, 1);
  }
  else /* my_pe == 0 */ {
    wis = fftw_export_wisdom_to_string();
    wislen = strlen(wis) + 1;
    MPI_Bcast(&wislen, 1, MPI_UNSIGNED_LONG_LONG, 0, comm);
    MPI_Bcast(wis, wislen, MPI_CHAR, 0, comm);
    fftw_free(wis);
  }
  MPI_Comm_free(&comm);
}

}