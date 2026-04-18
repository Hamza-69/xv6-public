#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define N              5        
#define M              10       
#define MIN_TX         5        
#define MAX_TX         10       
#define MAX_LOG        1024     
#define LOG_LINE_LEN   160      


static int      balance[N];                
static sem_t    acct_sem[N];               

static char     log_arr[MAX_LOG][LOG_LINE_LEN];
static int      log_count = 0;
static sem_t    log_sem;                   




static int rand_between(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}


static void log_record(const char *msg) {
    sem_wait(&log_sem);
    if (log_count < MAX_LOG) {
        strncpy(log_arr[log_count], msg, LOG_LINE_LEN - 1);
        log_arr[log_count][LOG_LINE_LEN - 1] = '\0';
        log_count++;
    }
    sem_post(&log_sem);
}


void *user_thread(void *arg) {
    long tid = (long)arg;                    
    int n_tx = rand_between(MIN_TX, MAX_TX);

    for (int i = 0; i < n_tx; i++) {
        int acct    = rand_between(0, N - 1);
        int amount  = rand_between(50, 300);
        int is_dep  = rand_between(0, 1);    
        char entry[LOG_LINE_LEN];

        
        sem_wait(&acct_sem[acct]);
        if (is_dep) {
            balance[acct] += amount;
            snprintf(entry, LOG_LINE_LEN,
                     "[OK]   T%02ld  DEPOSIT  %3d  -> acct %d  (new bal = %d)",
                     tid, amount, acct, balance[acct]);
        } else {
            if (balance[acct] >= amount) {
                balance[acct] -= amount;
                snprintf(entry, LOG_LINE_LEN,
                         "[OK]   T%02ld  WITHDRAW %3d <- acct %d  (new bal = %d)",
                         tid, amount, acct, balance[acct]);
            } else {
                snprintf(entry, LOG_LINE_LEN,
                         "[FAIL] T%02ld  WITHDRAW %3d <- acct %d  (bal only %d)",
                         tid, amount, acct, balance[acct]);
            }
        }
        sem_post(&acct_sem[acct]);
        

        printf("%s\n", entry);
        log_record(entry);

        usleep(rand_between(1000, 5000));    
    }
    pthread_exit(NULL);
}


int main(void) {
    pthread_t threads[M];
    setvbuf(stdout, NULL, _IOLBF, 0);   
    srand((unsigned)time(NULL));
    
    printf("  Concurrent Multi-Account Banking System\n");

    
    for (int i = 0; i < N; i++) {
        balance[i] = rand_between(500, 1500);
        sem_init(&acct_sem[i], 0, 1);        
        printf("Account %d initial balance = %d\n", i, balance[i]);
    }
    sem_init(&log_sem, 0, 1);
    printf("\nStarting %d user threads, each doing %d-%d transactions...\n\n",
           M, MIN_TX, MAX_TX);

    
    for (long t = 0; t < M; t++) {
        if (pthread_create(&threads[t], NULL, user_thread, (void *)t) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    
    for (int t = 0; t < M; t++) pthread_join(threads[t], NULL);

    printf("  FINAL BALANCES\n");
    for (int i = 0; i < N; i++)
        printf("Account %d : %d\n", i, balance[i]);
    printf("  TRANSACTION LOG  (%d entries)\n", log_count);
    for (int i = 0; i < log_count; i++)
        printf("%4d: %s\n", i, log_arr[i]);

    
    for (int i = 0; i < N; i++) sem_destroy(&acct_sem[i]);
    sem_destroy(&log_sem);
    return 0;
}
