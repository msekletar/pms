/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/*
   Implementation of Pipeline Merge Sort using OpenMPI

   Copyright (C) 2015  Michal Sekletar <xsekle00@stud.fit.vutbr.cz>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mpi.h>

#define INPUT_FILENAME "numbers"

static int mpi_rank;
static int mpi_world_size;

static int get_process_cmdline_path(pid_t pid, char **path) {
        long len;
        char *pid_cmdline_path = NULL;

        assert(pid > 0);
        assert(path);

        errno = 0;
        len = pathconf("/proc", _PC_PATH_MAX);
        if (len < 0)
                return errno ? -errno : -ENOTSUP;

        pid_cmdline_path = malloc(len + strlen("/proc/") + 1);
        if (!pid_cmdline_path)
                return -ENOMEM;

        bzero(pid_cmdline_path, len + strlen("/proc") + 1);

        sprintf(pid_cmdline_path, "/proc/%d/cmdline", pid);

        *path = pid_cmdline_path;

        return 0;
}

static int get_process_argv0(pid_t pid, char **argv0) {
        int r;
        char l[LINE_MAX] = {}, *cmdline_path = NULL, *_argv0 = NULL;
        FILE *f = NULL;

        assert(argv0);

        r = get_process_cmdline_path(pid, &cmdline_path);
        if (r < 0)
                goto out;

        f = fopen(cmdline_path, "re");
        if (!f) {
                r = -errno;
                goto out;
        }

        r = fscanf(f, "%s", l);
        if (r < 0) {
                r = -errno;
                goto out;
        }

        _argv0 = strdup(l);
        if (!_argv0) {
                r = -ENOMEM;
                goto out;
        }

        *argv0 = _argv0;

        r = 0;
 out:
        if (f)
                fclose(f);
        free(cmdline_path);
        return r;
}

static bool is_invoked_by_mpirun(void) {
        int r;
        pid_t ppid;
        char *argv0 = NULL;

        ppid = getppid();

        /* We are not intended to be run as daemon. Our original parent is gone */
        if (ppid == 1)
                return false;

        r = get_process_argv0(ppid, &argv0);
        if (r < 0)
                return false;

        /* If our parent has argv0 which contains mpirun string we return true.
           FIXME: Maybe more involved check is needed
        */
        r = !!strcasestr(argv0, "mpirun");

        free(argv0);

        return r;
}

static void mpi_init(int argc, char *argv[]) {
        MPI_Init(&argc, &argv);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
}

static void mpi_done(void) {
        MPI_Finalize();
}

static int read_input_file(off_t *count, unsigned char **numbers) {
        int r;
        FILE *f = NULL;
        struct stat st;
        unsigned char *_numbers = NULL;

        assert(count);
        assert(numbers);

        f = fopen(INPUT_FILENAME, "re");
        if (!f) {
                r = -errno;
                goto out;
        }

        r = fstat(fileno(f), &st);
        if (r < 0) {
                r = -errno;
                goto out;
        }

        _numbers = malloc(st.st_size);
        if (!_numbers) {
                r = -errno;
                goto out;
        }

        memset(_numbers, 0, st.st_size);

        r = fread(numbers, 1, st.st_size, f);
        if (r < 0) {
                r = ferror(f) ? -errno : -EIO;
                free(_numbers);
                goto out;
        }

        *count = st.st_size;
        *numbers = _numbers;

        r = 0;
 out:
        if (f)
                fclose(f);
        return r;
}

static void print_input(unsigned char *numbers, off_t count) {
        off_t i = 0;

        assert(numbers);
        assert(count >= 0);

        for (i = 0; i < count; ++i)
                printf("%u ", numbers[i]);

        printf("\n");

        fflush(stdout);
}

int main(int argc, char *argv[]) {
        int r;
        unsigned char *numbers = NULL;
        off_t count = 0;

        mpi_init(argc, argv);

        if (mpi_rank == 0) {
                bool t;

                t = is_invoked_by_mpirun();
                if (!t) {
                        fprintf(stderr, "This program is not meant to be invoked directly. "
                                         "Please run using attached script test.sh.\n");
                        MPI_Abort(MPI_COMM_WORLD, 1);
                        return EXIT_FAILURE;
                }

                r = read_input_file(&count, &numbers);
                if (r < 0) {
                        fprintf(stderr, "Failed to load data from input file: %m\n");
                        MPI_Abort(MPI_COMM_WORLD, 1);
                        return EXIT_FAILURE;
                }

                print_input(numbers, count);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        mpi_done();
        free(numbers);

        return EXIT_SUCCESS;
}
