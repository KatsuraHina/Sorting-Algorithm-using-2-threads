#include <stdio.h>              // For printf, fopen, fscanf, etc.
#include <stdlib.h>             // For malloc, free, exit
#include <pthread.h>            // For pthread functions
#include <unistd.h>             // For usleep (optional delay)

#define MAX_SIZE 200            // Maximum number of integers allowed in input

// Create aliases to make types shorter and cleaner
typedef pthread_mutex_t PMutex;
typedef pthread_cond_t PCond;

// Global array to store integers from file
int A[MAX_SIZE];
int n;                          // Number of integers read from the file
int total_swaps = 0;           // Total number of swaps done by both threads
int swap_t1 = 0;               // Number of swaps done by thread T1
int swap_t2 = 0;               // Number of swaps done by thread T2

// Thread synchronization
PMutex lock = PTHREAD_MUTEX_INITIALIZER;   // Mutex to protect shared resources
PCond cond_t1 = PTHREAD_COND_INITIALIZER;  // Condition variable for T1
PCond cond_t2 = PTHREAD_COND_INITIALIZER;  // Condition variable for T2

int turn = 1;                  // Whose turn it is: 1 = T1, 2 = T2
int stop = 0;                  // Flag to signal both threads to terminate

// The sorting function run by both threads
void* sort(void* arg) {
    int id = *(int*)arg;       // Get thread ID (1 for T1, 2 for T2)
    free(arg);                 // Free dynamically allocated ID

    printf("Thread T%d starting\n", id);

    while (1) {
        pthread_mutex_lock(&lock);  // Lock the mutex to enter critical section

        if (stop) {                 // If the stop flag is set, exit loop
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

        int local_swap = 0;   // Track number of swaps in this round

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
            swap_t1 = local_swap;       // Save T1's swap count
            total_swaps += local_swap;  // Update total swap count
            turn = 2;                   // Now it's T2's turn
            pthread_cond_signal(&cond_t2); // Wake up T2

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
            swap_t2 = local_swap;       // Save T2's swap count
            total_swaps += local_swap;  // Update total swap count

            // If both threads made 0 swaps, sorting is done
            if (swap_t1 == 0 && swap_t2 == 0) {
                stop = 1;                   // Set flag to terminate
                pthread_cond_signal(&cond_t1); // Wake up T1 if waiting
                pthread_cond_signal(&cond_t2); // Wake up T2 if waiting
            } else {
                turn = 1;               // Next round is T1's turn
                pthread_cond_signal(&cond_t1); // Wake up T1
            }
        }

        pthread_mutex_unlock(&lock);  // Exit critical section

        usleep(100000);               // Optional delay for visibility (100ms)
    }

    // Print result for this thread
    printf("Thread ID%d: total number of swaps = %d\n", id, (id == 1 ? swap_t1 : swap_t2));
    pthread_exit(NULL);              // Exit thread
}

int main(int argc, char* argv[]) {
    // Check if input file is provided
    if (argc != 2) {
        printf("Usage: %s ToSort\n", argv[0]);
        return 1;
    }

    // Open the input file for reading
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("File open failed");
        return 1;
    }

    // Read integers from file into array A
    n = 0;
    while (fscanf(file, "%d", &A[n]) == 1 && n < MAX_SIZE) {
        n++;
    }
    fclose(file);

    printf("Main thread: read %d integers\n", n);

    // Create thread IDs dynamically
    pthread_t t1, t2;
    int* id1 = malloc(sizeof(int));
    int* id2 = malloc(sizeof(int));
    *id1 = 1;
    *id2 = 2;

    printf("Main thread: starting threads...\n");

    // Create the two sorting threads
    pthread_create(&t1, NULL, sort, id1);
    pthread_create(&t2, NULL, sort, id2);

    // Wait for both threads to complete
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Print final sorted array
    printf("Sorted array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", A[i]);
    }
    printf("\nTotal number of swaps to sort array A = %d\n", total_swaps);

    return 0;   // Program completed successfully
}
