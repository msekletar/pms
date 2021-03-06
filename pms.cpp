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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include <list>
#include <queue>

#include <mpi.h>

using namespace std;

enum {
        _QUEUE_INVALID = -1,
        QUEUE1,
        QUEUE2,
        _QUEUE_MAX
};

struct SendCommunication {
        MPI_Request *mpi_request;
        unsigned char *buf;
};

static int mpi_rank;
static int mpi_world_size;
static list<SendCommunication> send_communications;
static const char *INPUT_FILENAME = "numbers";

static bool is_finished(SendCommunication *c) {
        int flag = 0;

        if (c->mpi_request)
                MPI_Request_get_status(*c->mpi_request, &flag, MPI_STATUS_IGNORE);

        return !!flag;
}

static int get_process_cmdline_path(pid_t pid, char **path) {
        long len;
        char *pid_cmdline_path = NULL;

        assert(pid > 0);
        assert(path);

        errno = 0;
        len = pathconf("/proc", _PC_PATH_MAX);
        if (len < 0)
                return errno ? -errno : -ENOTSUP;

        pid_cmdline_path = (char *) malloc(len + strlen("/proc/") + 1);
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

static void mpi_err_handler(MPI_Comm *c, int *status, ...) {
        fprintf(stderr, "Encountered an unrecoverable runtime error. Execution aborted.\n");

        abort();
}

static void mpi_init(int argc, char *argv[]) {
        static MPI_Errhandler handler;

        MPI_Init(&argc, &argv);

        MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

        /* In case of communication error algorithm might produce incorrect result.
           If that happens we shouldn't continue and abort execution
        */
        MPI_Errhandler_create(mpi_err_handler, &handler);
        MPI_Comm_set_errhandler(MPI_COMM_WORLD, handler);
}

static void mpi_done(void) {
        MPI_Finalize();
}

static int read_input_file(int *count, unsigned char **numbers) {
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

        _numbers = (unsigned char *) malloc(st.st_size);
        if (!_numbers) {
                r = -errno;
                goto out;
        }

        memset(_numbers, 0, st.st_size);

        r = fread(_numbers, st.st_size, 1, f);
        if (r < 0) {
                r = ferror(f) ? -errno : -EIO;
                free(_numbers);
                goto out;
        }

        *count = (int) st.st_size;
        *numbers = _numbers;

        r = 0;
 out:
        if (f)
                fclose(f);
        return r;
}

static void print_input(unsigned char *numbers, int count) {
        int i = 0;

        assert(numbers);
        assert(count >= 0);

        for (i = 0; i < count; ++i)
                printf("%u ", numbers[i]);

        printf("\n");

        fflush(stdout);
}

static void input_processor(unsigned char *numbers, int count) {
        int r, i, j;
        MPI_Request send_requests[count];

        assert(numbers);
        assert(count > 0);
        assert((int) (log(count)/log(2)) + 1 == mpi_world_size);

        /* We are reading input buffer from the end so that numbers at the end are sent first */
        for (i = count - 1, j = 0; i >= 0; --i, ++j)
                MPI_Isend(&numbers[i],
                          1,
                          MPI_UNSIGNED_CHAR,
                          mpi_rank + 1,
                          j % _QUEUE_MAX,
                          MPI_COMM_WORLD,
                          &send_requests[j]);

        r = MPI_Waitall(count, send_requests, MPI_STATUSES_IGNORE);
        if (r != MPI_SUCCESS)
                MPI_Abort(MPI_COMM_WORLD, -EPIPE);
}

static int queue_receive_n(queue<unsigned char>* q, int n, int queue_id) {
        int received = 0;

        assert(q);
        assert(n >= 0);

        while (received < n) {
                MPI_Status recv_status;
                unsigned char e;

                MPI_Recv(&e,
                         1,
                         MPI_UNSIGNED_CHAR,
                         mpi_rank - 1,
                         queue_id,
                         MPI_COMM_WORLD,
                         &recv_status);

                q->push(e);
                ++received;

                assert(recv_status.MPI_SOURCE == mpi_rank - 1);
                assert(recv_status.MPI_TAG == queue_id);
        }

        return 0;
}

static void print_n_queue_elements(queue<unsigned char> *q, int n) {

        for (int i = 0; i < n; ++i) {
                printf("%d\n", (int)  q->front());
                q->pop();
        }
}

static int queue_send_n(queue<unsigned char> *q, int n, int queue_id, unsigned char *send_buffers, MPI_Request *mpi_requests) {
        int sent = 0;
        static int i = 0;

        assert(q);
        assert(n >= 0);
        assert(queue_id < _QUEUE_MAX);

        if (n == 0)
                return 0;

        /* last processor doesn't send data, but it prints them to standard output */
        if (mpi_rank == mpi_world_size - 1) {
                print_n_queue_elements(q, n);
                return 0;
        }

        while (sent < n) {
                MPI_Request *r;
                unsigned char *send_buffer;
                SendCommunication c;

                send_buffer = send_buffers + i;
                r = mpi_requests + i;

                *send_buffer = q->front();
                q->pop();

                MPI_Isend(send_buffer,
                          1,
                          MPI_UNSIGNED_CHAR,
                          mpi_rank + 1,
                          queue_id,
                          MPI_COMM_WORLD,
                          r);

                c.buf = send_buffer;
                c.mpi_request = r;

                send_communications.push_back(c);
                ++sent;
                ++i;
        }

        return 0;
}

static void wait_for_communications(list<SendCommunication> *send_communications) {
        auto i = send_communications->begin();

        if (mpi_rank == mpi_world_size -1)
                return;

        while (!send_communications->empty()) {
                while (i != send_communications->end())
                        if (is_finished(&*i))
                                send_communications->erase(i++);
        }
}

static void merging_processor(int count) {
        unsigned char *send_buffers = NULL;
        MPI_Request *mpi_requests = NULL;
        int processed = 0, queue_id;
        size_t max_queue_len;
        queue<unsigned char> queues[2];

        assert((int) (log(count)/log(2)) + 1 == mpi_world_size);

        /* allocate resource for async communication */
        send_buffers = (unsigned char *) calloc(count, sizeof(unsigned char));
        if (!send_buffers)
                abort();

        mpi_requests = (MPI_Request *) calloc(count, sizeof(MPI_Request));
        if (!mpi_requests)
                abort();

        /* we start receiving data from QUEUE1 */
        queue_id = QUEUE1;

        /* each processor merges two queues, and maximum length of each is 2^(rank - 1) */
        max_queue_len = 1 << (mpi_rank - 1);

        /* algorithm finishes when processor seen all elements from input sequence */
        while (processed < count) {
                /* in each iteration is pointers Q1 and Q2 alternates */
                unsigned q1_processed = 0, q2_processed = 0;
                int Q1_id = queue_id % _QUEUE_MAX;
                int Q2_id = (queue_id + 1) % _QUEUE_MAX;
                queue<unsigned char>* Q1 = &queues[Q1_id];
                queue<unsigned char>* Q2 = &queues[Q2_id];


                /* algorithm requires first queue to be full to proceed */
                if (Q1->size() < max_queue_len)
                        queue_receive_n(Q1, max_queue_len - Q1->size(), Q1_id);

                /* in the other queue there must be at least one element */
                if (Q2->size() == 0)
                        queue_receive_n(Q2, 1, Q2_id);

                /* compare first element from each queue and pass the smaller one to the next processor */
                while (q1_processed < max_queue_len && q2_processed < max_queue_len) {

                        if (Q1->front() < Q2->front()) {
                                queue_send_n(Q1, 1, Q1_id, send_buffers, mpi_requests);
                                ++q1_processed;
                        } else {
                                queue_send_n(Q2, 1, Q1_id, send_buffers, mpi_requests);
                                ++q2_processed;

                                if (q2_processed != max_queue_len)
                                        queue_receive_n(Q2, 1, Q2_id);
                        }
                }

                /* at least one queue must be empty at this point */
                assert(Q1->empty() || Q2->empty());

                /* process rest of the other queue before starting next iteration */
                if (!Q1->empty()) {
                        queue_send_n(Q1, Q1->size(), Q1_id, send_buffers, mpi_requests);
                } else {
                        /* we handle Q2 a bit differnt because we are receiveing on it one element at a time */
                        q2_processed += Q2->size();

                        /* send what is already queued up */
                        queue_send_n(Q2, Q2->size(), Q1_id, send_buffers, mpi_requests);

                        /* receive elements possibly waiting in the input buffer */
                        queue_receive_n(Q2, max_queue_len - q2_processed, Q2_id);

                        q2_processed += Q2->size();

                        /* send them down the pipeline */
                        queue_send_n(Q2, Q2->size(), Q1_id, send_buffers, mpi_requests);

                }

                /* increment queue_id counter so Q1 becomes Q2 and the other-way around on next iteration  */
                queue_id++;

                /* increment processed counter, ensures finiteness of the algorithm */
                processed += 2 * max_queue_len;
        }

        wait_for_communications(&send_communications);
        free(mpi_requests);
        free(send_buffers);
}

void pipeline_merge_sort(unsigned char *numbers, int count) {

        if (mpi_rank == 0)
                input_processor(numbers, count);
        else
                merging_processor(1 << (mpi_world_size - 1));
}

int main(int argc, char *argv[]) {
        int r;
        unsigned char *numbers = NULL;
        int count = 0;

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

        /* make sure that input processor has enough time to print input sequence */
        if (mpi_rank != 0)
                usleep(10000);

        pipeline_merge_sort(numbers, count);

        MPI_Barrier(MPI_COMM_WORLD);

        mpi_done();
        free(numbers);

        return EXIT_SUCCESS;
}
