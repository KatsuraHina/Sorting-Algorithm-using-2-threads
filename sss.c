#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_SIZE 200

typedef pthread_mutex_t PMutex;
typedef pthread_cond_t PCond;

int A[MAX_SIZE];
int n;
int total_swaps = 0;
int swap_t1 = 0;
int swap_t2 = 0;

PMutex lock = PTHREAD_MUTEX_INITIALIZER;
PCond cond_t1 = PTHREAD_COND_INITIALIZER;
PCond cond_t2 = PTHREAD_COND_INITIALIZER;
PMutex main_mutex = PTHREAD_MUTEX_INITIALIZER;
PCond cond_main = PTHREAD_COND_INITIALIZER;

int turn = 1;
int stop = 0;

void* sort(void* arg) {
    int id = *(int*)arg;
    free(arg);

    printf("Thread T%d starting\n", id);

    while (1) {
        pthread_mutex_lock(&lock);

        if (stop) {
            pthread_mutex_unlock(&lock);
            break;
        }

        // Wait until it's this thread's turn
        while (turn != id && !stop) {
            if (id == 1)
                pthread_cond_wait(&cond_t1, &lock);
            else
                pthread_cond_wait(&cond_t2, &lock);
        }

        int local_swap = 0;

        if (id == 1) {
            // Thread T1 compares A[2i] and A[2i+1]
            for (int i = 0; i <= (n - 1) / 2; i++) {
                int idx = 2 * i;
                if (idx + 1 < n && A[idx] > A[idx + 1]) {
                    int temp = A[idx];
                    A[idx] = A[idx + 1];
                    A[idx + 1] = temp;
                    local_swap++;
                }
            }
            swap_t1 = local_swap;
            total_swaps += local_swap;
            turn = 2;
            pthread_cond_signal(&cond_t2);  // Signal T2
        } else {
            // Thread T2 compares A[2i-1] and A[2i]
            for (int i = 1; i <= (n - 1) / 2; i++) {
                int idx = 2 * i - 1;
                if (idx + 1 < n && A[idx] > A[idx + 1]) {
                    int temp = A[idx];
                    A[idx] = A[idx + 1];
                    A[idx + 1] = temp;
                    local_swap++;
                }
            }
            swap_t2 = local_swap;
            total_swaps += local_swap;

            if (swap_t1 == 0 && swap_t2 == 0) {
                stop = 1;
                pthread_cond_signal(&cond_t1);
                pthread_cond_signal(&cond_t2);
            } else {
                turn = 1;
                pthread_cond_signal(&cond_t1);  // Signal T1
            }
        }

        pthread_mutex_unlock(&lock);

         // Signal the main thread after each iteration
        pthread_mutex_lock(&main_mutex);
        pthread_cond_signal(&cond_main);
        pthread_mutex_unlock(&main_mutex);


       usleep(100000);
    }

    printf("Thread ID%d: total number of swaps = %d\n", id, (id == 1 ? swap_t1 : swap_t2));
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s ToSort\n", argv[0]);
        return 1;
    }

    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("File open failed");
        return 1;
    }

    n = 0;
    while (fscanf(file, "%d", &A[n]) == 1 && n < MAX_SIZE) {
        n++;
    }
    fclose(file);

    printf("Main thread: read %d integers\n", n);

    pthread_t t1, t2;
    int* id1 = malloc(sizeof(int));
    int* id2 = malloc(sizeof(int));
    *id1 = 1;
    *id2 = 2;

    printf("Main thread: starting threads...\n");

    pthread_create(&t1, NULL, sort, id1);
    pthread_create(&t2, NULL, sort, id2);

    // Main thread loop to wait and check 'stop'
    pthread_mutex_lock(&main_mutex);
    while (!stop) {
        pthread_cond_wait(&cond_main, &main_mutex);
    }
    pthread_mutex_unlock(&main_mutex);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Sorted array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", A[i]);
    }
    printf("\nTotal number of swaps to sort array A = %d\n", total_swaps);

    free(id1);
    free(id2);

    return 0;
}