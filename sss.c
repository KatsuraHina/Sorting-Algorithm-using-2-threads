#include <stdio.h>      
#include <stdlib.h>     
#include <pthread.h>   
#include <unistd.h>    

#define MAX_SIZE 200    

// typedefs because im too lazy to keep typing the same stuff everytime
typedef pthread_mutex_t PMutex;
typedef pthread_cond_t  PCond;

// My Global Variable
int A[MAX_SIZE];    
int n;              

int swap = 0;       

int t1_swaps = 0;   
int t2_swaps = 0;   

int curr_swap_t1 = 0; 
int curr_swap_t2 = 0; 

// Syncronization
PMutex lock       = PTHREAD_MUTEX_INITIALIZER;
PCond  cond_t1    = PTHREAD_COND_INITIALIZER;
PCond  cond_t2    = PTHREAD_COND_INITIALIZER;

// For making main thread wait
PMutex main_mtx     = PTHREAD_MUTEX_INITIALIZER;
PCond  main_cv      = PTHREAD_COND_INITIALIZER;
int    workers_done = 0;

int turn = 1;    
int stop_flag = 0;  

// To run the threadss
void* sort(void* arg) {
    int id = *((int*)arg); 
    free(arg); 
    printf("Thread T%d starting\n", id);

    while (1) { 
        pthread_mutex_lock(&lock);
        if (stop_flag) {
            pthread_mutex_unlock(&lock); 
            break; 
        }

        // Wait for my turn
        while (turn != id && !stop_flag) {
            pthread_cond_wait(id==1 ? &cond_t1 : &cond_t2, &lock);
        }

        if (stop_flag) {
            pthread_mutex_unlock(&lock);
            break;
        }
        // Count swaps for this pass
        int local_swap = 0; 

        if (id == 1) {
            for (int i = 0; i <= (n - 1) / 2; i++) {
                int idx = 2*i;
                // CHeck if the order is right
                if (idx+1 < n && A[idx] > A[idx+1]) {
                    // Swap
                    int tmp = A[idx];
                    A[idx] = A[idx+1];
                    A[idx+1] = tmp;
                    local_swap++; // Swap Count
                }
            }
            curr_swap_t1 = local_swap; // Store the swap
            t1_swaps    += local_swap; // Add total to t1
            swap        += local_swap; // Add to overall total

            turn = 2; // Set turn to T2
            pthread_cond_signal(&cond_t2); 

        } else { 
            // compare A[2i-1] and A[2i]
            for (int i = 1; i <= (n - 1) / 2; i++) {
                int idx = 2*i - 1;
                // To check if the elements are in order
                if (idx+1 < n && A[idx] > A[idx+1]) {
                    // Swap them
                    int tmp = A[idx];
                    A[idx] = A[idx+1];
                    A[idx+1] = tmp;
                    local_swap++;
                }
            }
            curr_swap_t2 = local_swap; 
            t2_swaps    += local_swap; 
            swap        += local_swap; 

            // Check if we are done sorting
            if (curr_swap_t1 == 0 && curr_swap_t2 == 0) {
                stop_flag = 1; // Set the flag to stop everyone
                pthread_cond_signal(&cond_t1); // Wake up T1 (so it can see stop_flag)

                // Tell the main thread that we are done
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
        usleep(100000); 
    }

    
    printf("Thread T%d: total number of swaps = %d\n",
           id, (id == 1 ? t1_swaps : t2_swaps));
    return NULL; 
}
// Arg Check
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s ToSort\n", argv[0]);
        return 1; 
    }

    FILE* f = fopen(argv[1], "r");
    if (!f) {
        perror("Failed to open file.  "); 
        return 1; 
    }

    // Read numbers from file
    n = 0;
    while (n < MAX_SIZE && fscanf(f, "%d", &A[n]) == 1) {
        n++;
    }
    fclose(f); 

    printf("Main thread: read %d integers\n", n);

    // Thread variables
    pthread_t t1, t2;
    int *id1 = malloc(sizeof(int)); 
    int *id2 = malloc(sizeof(int)); 
    *id1 = 1; 
    *id2 = 2; 

    printf("Main thread: starting threads...\n");
    // Create the threads
    pthread_create(&t1, NULL, sort, id1);
    pthread_create(&t2, NULL, sort, id2);

    // For Threads to wait
    pthread_mutex_lock(&main_mtx);
    while (!workers_done) {
        pthread_cond_wait(&main_cv, &main_mtx); 
    }
    pthread_mutex_unlock(&main_mtx); 

    // Clean up threads
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Print the final sorted array
    printf("Sorted array: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", A[i]);
    }
    printf("\nTotal number of swaps to sort array A = %d\n", swap);

    return 0; 