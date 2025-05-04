#include <stdio.h>              // printf, fopen, fscanf, perror
#include <stdlib.h>             // malloc, free, exit
#include <pthread.h>            // pthread APIs
#include <unistd.h>             // usleep (optional)

/* Maximum number of integers in the input file */
#define MAX_SIZE 200

/* Shorter type aliases */
typedef pthread_mutex_t PMutex;
typedef pthread_cond_t  PCond;

/* Shared data */
int A[MAX_SIZE];
int n;                // number of ints read
int swap = 0;         // total swap count
int swap_t1 = 0;      // swaps by T1
int swap_t2 = 0;      // swaps by T2

/* Synchronization primitives */
PMutex lock       = PTHREAD_MUTEX_INITIALIZER;
PCond  cond_t1    = PTHREAD_COND_INITIALIZER;
PCond  cond_t2    = PTHREAD_COND_INITIALIZER;

/* For the “main thread must block/unblock” requirement */
PMutex main_mtx   = PTHREAD_MUTEX_INITIALIZER;
PCond  main_cv    = PTHREAD_COND_INITIALIZER;
int    workers_done = 0;   // set to 1 when sorting is over

/* Who’s turn? 1=T1’s turn, 2=T2’s turn */
int turn = 1;

/* Stop flag once two zero-swap passes have occurred */
int stop_flag = 0;

/* The sorting function run by both threads */
void* sort(void* arg) {
    int id = *((int*)arg);  // SAFELY unpack the thread ID
    free(arg);

    printf("Thread T%d starting\n", id);

    while (1) {
        pthread_mutex_lock(&lock);

        if (stop_flag) {
            pthread_mutex_unlock(&lock);
            break;
        }

        /* wait for our turn */
        while (turn != id && !stop_flag) {
            if (id == 1)
                pthread_cond_wait(&cond_t1, &lock);
            else
                pthread_cond_wait(&cond_t2, &lock);
        }
        if (stop_flag) {
            pthread_mutex_unlock(&lock);
            break;
        }

        /* do this thread’s pass */
        int local_swap = 0;
        if (id == 1) {
            /* T1: compare A[2i] vs A[2i+1] */
            for (int i = 0; i <= (n - 1) / 2; i++) {
                int idx = 2 * i;
                if (idx + 1 < n && A[idx] > A[idx + 1]) {
                    int tmp = A[idx];
                    A[idx] = A[idx + 1];
                    A[idx + 1] = tmp;
                    local_swap++;
                }
            }
            swap_t1 = local_swap;
            swap    += local_swap;

            /* hand off to T2 */
            turn = 2;
            pthread_cond_signal(&cond_t2);

        } else {
            /* T2: compare A[2i-1] vs A[2i] */
            for (int i = 1; i <= (n - 1) / 2; i++) {
                int idx = 2 * i - 1;
                if (idx + 1 < n && A[idx] > A[idx + 1]) {
                    int tmp = A[idx];
                    A[idx] = A[idx + 1];
                    A[idx + 1] = tmp;
                    local_swap++;
                }
            }
            swap_t2 = local_swap;
            swap    += local_swap;

            /* termination check: two zero-swap passes in a row */
            if (swap_t1 == 0 && swap_t2 == 0) {
                stop_flag = 1;
                /* wake T1 if it’s waiting */
                pthread_cond_signal(&cond_t1);
                /* unblock main thread */
                pthread_mutex_lock(&main_mtx);
                workers_done = 1;
                pthread_cond_signal(&main_cv);
                pthread_mutex_unlock(&main_mtx);
            } else {
                turn = 1;
                pthread_cond_signal(&cond_t1);
            }
        }

        pthread_mutex_unlock(&lock);
        usleep(100000);  // optional delay for clarity
    }

    printf("Thread T%d: total number of swaps = %d\n",
           id, (id == 1 ? swap_t1 : swap_t2));
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s ToSort\n", argv[0]);
        return 1;
    }

    /* read file */
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("File open failed");
        return 1;
    }
    n = 0;
    while (n < MAX_SIZE && fscanf(file, "%d", &A[n]) == 1) {
        n++;
    }
    fclose(file);

    printf("Main thread: read %d integers\n", n);

    /* spawn T1 and T2 */
    pthread_t t1, t2;
    int* id1 = malloc(sizeof(int));
    int* id2 = malloc(sizeof(int));
    *id1 = 1;  *id2 = 2;

    printf("Main thread: starting threads...\n");
    pthread_create(&t1, NULL, sort, id1);
    pthread_create(&t2, NULL, sort, id2);

    /* block until worker signals completion */
    pthread_mutex_lock(&main_mtx);
    while (!workers_done)
        pthread_cond_wait(&main_cv, &main_mtx);
    pthread_mutex_unlock(&main_mtx);

    /* clean up */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    /* final output */
    printf("Sorted array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", A[i]);
    }
    printf("\nTotal number of swaps to sort array A = %d\n", swap);

    return 0;
}
