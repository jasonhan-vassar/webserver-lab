#include "io_helper.h"
#include "request.h"
// #include "request_fifo.h"
// #include "request_priority_queue.h"
#include "request_ultra.h"
#include <pthread.h>
#include <stdio.h>

// struct fifo fifo;
// struct priority_queue pq;

int max_buffer_size;
struct node *head;
volatile int request_count;

pthread_cond_t arrived = PTHREAD_COND_INITIALIZER;
pthread_cond_t used = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_connection(void *arg) {
    sleep(5);

    while (true) {
        pthread_mutex_lock(&mutex);
        while (request_count == 0)
            pthread_cond_wait(&arrived, &mutex);
        // struct request request = get(&fifo);
        // struct request request2 = dequeue(&pq);
        struct request request3 = pop(&head);
        request_count--;
        // printf("%s size: %lld\n", request2.filename, request2.sbuf.st_size);
        // printf("%s size: %lld\n", request3.filename, request3.sbuf.st_size);
        pthread_mutex_unlock(&mutex);

        // sleep(3); // TODO: remove; tests multithreading
        request_handle(&request3);

        pthread_mutex_lock(&mutex);
        close_or_die(request3.fd);

        pthread_cond_signal(&used);

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int c;
    char *root_dir = "."; // default root
    int port = 10000;
    int threads = 1;
    max_buffer_size = 1;
    char *schedalg = "FIFO";

    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1) {
        switch (c) {
        case 'd':
            root_dir = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            threads = atoi(optarg);
            break;
        case 'b':
            max_buffer_size = atoi(optarg);
            break;
        case 's':
            schedalg = optarg;
            break;
        default:
            fprintf(stderr, "usage: wserver [-d basedir] [-p port] [-t "
                            "threads] [-b buffers] [-s schedalg]\n");
            exit(1);
        }
    }

    chdir_or_die(root_dir);

    // init_fifo(&fifo);
    // init_priority_queue(&pq);
    head = NULL;
    request_count = 0;

    pthread_t workers[threads];
    for (int i = 0; i < threads; i++)
        pthread_create(&workers[i], NULL, handle_connection, NULL);

    int listen_fd = open_listen_fd_or_die(port);
    while (true) {
        pthread_mutex_lock(&mutex);
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        pthread_mutex_unlock(&mutex);

        int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr,
                                    (socklen_t *) &client_len);

        pthread_mutex_lock(&mutex);
        while (request_count == max_buffer_size)
            pthread_cond_wait(&used, &mutex);

        struct request request;
        request_parse(conn_fd, &request);

        // put(&fifo, request);
        // enqueue(&pq, request);

        if (strcmp(schedalg, "FIFO") == 0)
            push(&head, request);
        else
            push_ordered(&head, request);
        request_count++;
        printf("%s size: %lld\n", request.filename, request.sbuf.st_size);

        pthread_cond_broadcast(&arrived);

        pthread_mutex_unlock(&mutex);
    }

    return 0;
}
