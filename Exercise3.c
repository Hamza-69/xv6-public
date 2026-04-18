

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFSZ            5      
#define N_PRODUCERS      4
#define N_CONSUMERS      4      
#define N_READERS        9      
#define TOTAL_THREADS    (N_PRODUCERS + N_CONSUMERS + N_READERS)
#define OPS_PER_THREAD   4      


static int buf[BUFSZ];
static int in = 0, out = 0;
static int count = 0;           


static int   rc = 0;            
static sem_t m;                 
static sem_t wrt;               
static sem_t order_s;           
static sem_t empty_s;           
static sem_t full_s;            
static sem_t prn;               

static int next_item = 1;       

static int rand_between(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}


void *producer(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int item;
        sem_wait(&empty_s);                  
        sem_wait(&order_s);                  
        sem_wait(&wrt);                      
        sem_post(&order_s);                  
        item        = next_item++;
        buf[in]     = item;
        in          = (in + 1) % BUFSZ;
        count++;
        sem_wait(&prn);
        printf("[P%-2ld] PRODUCED item %3d  (count=%d)\n", id, item, count);
        sem_post(&prn);
        sem_post(&wrt);
        sem_post(&full_s);
        usleep(rand_between(2000, 8000));
    }
    pthread_exit(NULL);
}


void *consumer(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int item;
        sem_wait(&full_s);                   
        sem_wait(&order_s);                  
        sem_wait(&wrt);                      
        sem_post(&order_s);                  
        item        = buf[out];
        out         = (out + 1) % BUFSZ;
        count--;
        sem_wait(&prn);
        printf("[C%-2ld] CONSUMED item %3d  (count=%d)\n", id, item, count);
        sem_post(&prn);
        sem_post(&wrt);
        sem_post(&empty_s);
        usleep(rand_between(2000, 8000));
    }
    pthread_exit(NULL);
}


void *reader(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        sem_wait(&order_s);                  
        sem_wait(&m);
        rc++;
        if (rc == 1) sem_wait(&wrt);         
        sem_post(&m);
        sem_post(&order_s);                  

        sem_wait(&prn);
        printf("[R%-2ld] READING  count=%d  rc=%d  buf=[", id, count, rc);
        for (int k = 0; k < count; k++) {
            int idx = (out + k) % BUFSZ;
            printf("%d%s", buf[idx], (k == count - 1) ? "" : ",");
        }
        printf("]\n");
        sem_post(&prn);

        usleep(rand_between(1000, 4000));

        sem_wait(&m);
        rc--;
        if (rc == 0) sem_post(&wrt);         
        sem_post(&m);

        usleep(rand_between(1000, 4000));
    }
    pthread_exit(NULL);
}

int main(void) {
    pthread_t threads[TOTAL_THREADS];
    setvbuf(stdout, NULL, _IOLBF, 0);   
    srand((unsigned)time(NULL));

    printf("  Producers / Consumers / Readers   BUFSZ=%d   threads=%d\n",
           BUFSZ, TOTAL_THREADS);

    sem_init(&m,       0, 1);
    sem_init(&wrt,     0, 1);
    sem_init(&order_s, 0, 1);
    sem_init(&empty_s, 0, BUFSZ);
    sem_init(&full_s,  0, 0);
    sem_init(&prn,     0, 1);

    
    int  pool[TOTAL_THREADS];
    int  k = 0;
    for (int i = 0; i < N_PRODUCERS; i++) pool[k++] = 0;   
    for (int i = 0; i < N_CONSUMERS; i++) pool[k++] = 1;   
    for (int i = 0; i < N_READERS;   i++) pool[k++] = 2;   
    
    for (int i = TOTAL_THREADS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }

    int p_id = 0, c_id = 0, r_id = 0;
    for (long t = 0; t < TOTAL_THREADS; t++) {
        void *(*fn)(void *);
        long  id;
        switch (pool[t]) {
        case 0:  fn = producer; id = p_id++; printf("  spawn P%ld\n", id); break;
        case 1:  fn = consumer; id = c_id++; printf("  spawn C%ld\n", id); break;
        default: fn = reader;   id = r_id++; printf("  spawn R%ld\n", id); break;
        }
        if (pthread_create(&threads[t], NULL, fn, (void *)id) != 0) {
            perror("pthread_create"); exit(1);
        }
    }
    printf("  total: %d producers, %d consumers, %d readers\n\n",
           p_id, c_id, r_id);

    for (int t = 0; t < TOTAL_THREADS; t++) pthread_join(threads[t], NULL);

    printf("  done.  final buffer count = %d\n", count);

    sem_destroy(&m);
    sem_destroy(&wrt);
    sem_destroy(&order_s);
    sem_destroy(&empty_s);
    sem_destroy(&full_s);
    sem_destroy(&prn);
    return 0;
}
