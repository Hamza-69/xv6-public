

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_LINE 1024


static pthread_mutex_t read_mtx = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
    int   read_fd;           
    int   write_fd;          
    char *target_word;       
} thread_arg_t;


static ssize_t read_line_locked(int fd, char *buf, size_t cap) {
    size_t idx = 0;
    char c;
    ssize_t r;
    while (idx < cap - 1 && (r = read(fd, &c, 1)) > 0) {
        buf[idx++] = c;
        if (c == '\n') break;
    }
    buf[idx] = '\0';
    if (idx == 0 && r <= 0) return r;   
    return (ssize_t)idx;
}


static int count_in_line(const char *line, const char *word) {
    int count = 0;
    size_t wlen = strlen(word);
    const char *p = line;
    while ((p = strstr(p, word)) != NULL) {
        int left_ok  = (p == line) || !isalnum((unsigned char)*(p - 1));
        int right_ok = !isalnum((unsigned char)*(p + wlen));
        if (left_ok && right_ok) count++;
        p += wlen;
    }
    return count;
}


void *worker(void *arg) {
    thread_arg_t *a = (thread_arg_t *)arg;

    
    printf("From inside the thread: read from lines_pipe at: %d and "
           "write to counts_pipe at %d\n", a->read_fd, a->write_fd);

    
    char    buf[MAX_LINE];
    int     count = 0;
    ssize_t n;

    for (;;) {
        pthread_mutex_lock(&read_mtx);
        n = read_line_locked(a->read_fd, buf, MAX_LINE);
        pthread_mutex_unlock(&read_mtx);

        if (n <= 0) break;          
        count += count_in_line(buf, a->target_word);
    }

    
    if (write(a->write_fd, &count, sizeof(int)) != sizeof(int))
        perror("worker: write");
    close(a->write_fd);     
    close(a->read_fd);      

    free(a);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);   
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <filename> <word> <num_threads>\n", argv[0]);
        return 1;
    }
    const char *fname       = argv[1];
    const char *word        = argv[2];
    int         num_threads = atoi(argv[3]);
    if (num_threads <= 0) { fprintf(stderr, "num_threads must be > 0\n"); return 1; }

    printf("File: %s  Word: \"%s\"  Threads: %d\n\n", fname, word, num_threads);

    
    int file_fd = open(fname, O_RDONLY);
    if (file_fd < 0) { perror("open"); return 1; }

    
    int lines_pipe[2], counts_pipe[2];
    if (pipe(lines_pipe)  < 0) { perror("pipe"); return 1; }
    if (pipe(counts_pipe) < 0) { perror("pipe"); return 1; }

    
    pthread_t *tids = malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        thread_arg_t *a = malloc(sizeof(thread_arg_t));
        a->read_fd     = dup(lines_pipe[0]);   
        a->write_fd    = dup(counts_pipe[1]);  
        a->target_word = (char *)word;
        if (pthread_create(&tids[i], NULL, worker, a) != 0) {
            perror("pthread_create"); return 1;
        }
    }

    
    char   linebuf[MAX_LINE];
    size_t idx = 0;
    char   c;
    ssize_t r;
    while ((r = read(file_fd, &c, 1)) > 0) {
        linebuf[idx++] = c;
        if (c == '\n' || idx == MAX_LINE) {
            if (write(lines_pipe[1], linebuf, idx) < 0) { perror("write lines"); }
            idx = 0;
        }
    }
    if (idx > 0) {                 
        linebuf[idx++] = '\n';
        write(lines_pipe[1], linebuf, idx);
    }

    
    
    close(lines_pipe[1]);
    close(lines_pipe[0]);          
    close(file_fd);

    
    close(counts_pipe[1]);

    
    int total = 0, partial;
    while ((r = read(counts_pipe[0], &partial, sizeof(int))) > 0) {
        total += partial;
    }
    close(counts_pipe[0]);

    
    for (int i = 0; i < num_threads; i++) pthread_join(tids[i], NULL);
    free(tids);

    printf("\nTotal occurrences of \"%s\" in %s : %d\n", word, fname, total);
    return 0;
}
