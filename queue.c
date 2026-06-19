#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

typedef struct Node {
    char* file_path;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
    int size;
    pthread_mutex_t queue_lock;
    pthread_cond_t cond_variable;
} Queue;

typedef struct ThreadArgs {
    Queue* queue;
    const char* target_word;
    int* total_matches;
    pthread_mutex_t matches_lock;
} ThreadArgs;

int number_of_thread = 3;
int total_matches = 0;
char buffer[512] = "Error";

Queue* create_queue() {
    Queue *q = (Queue*) malloc(sizeof(Queue));

    if(q == NULL) {
        printf("Memory Allocation Encountered with an Error!\n");
        return NULL;
    }
    
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    pthread_mutex_init(&q->queue_lock, NULL);
    pthread_cond_init(&q->cond_variable, NULL);

    return q;
}

int empty(Queue* q) {
    return q->size == 0;
}

void enqueue(Queue* q, char* path) {
    pthread_mutex_lock(&q->queue_lock);
    Node* new_node = (Node*) malloc(sizeof(Node));

    if(new_node == NULL) {
        printf("Memory Allocation Encountered with an Error!\n");
        pthread_mutex_unlock(&q->queue_lock);
        return;
    }

    new_node->file_path = strdup(path);
    new_node->next = NULL;

    if(empty(q)){
        q->head = q->tail = new_node;
    }
    else {
        q->tail->next = new_node;
        q->tail = new_node;
    }

    q->size++;

    pthread_mutex_unlock(&q->queue_lock);
    pthread_cond_signal(&q->cond_variable);
}

char* dequeue(Queue* q) {
    pthread_mutex_lock(&q->queue_lock);

    while(empty(q)) pthread_cond_wait(&q->cond_variable, &q->queue_lock);

    Node* temp_node = q->head;
    if(q->size == 1) {
        q->head = q->tail = NULL;
    }else {
        q->head = q->head->next;
    }
    
    q->size--;
    char* file_path = temp_node->file_path;

    free(temp_node);
    pthread_mutex_unlock(&q->queue_lock);

    return file_path;
}

void free_queue(Queue* q) {
    if(q == NULL) return;

    pthread_mutex_lock(&q->queue_lock);

    Node* current_node = q->head;
    while(current_node != NULL) {
        Node* next_node = current_node->next;

        free(current_node->file_path);
        free(current_node);

        current_node = next_node;
    }

    q->head = q->tail = NULL;
    q->size = 0;

    pthread_mutex_unlock(&q->queue_lock);

    pthread_mutex_destroy(&q->queue_lock);
    pthread_cond_destroy(&q->cond_variable);
    free(q);
}

int count_words_in_line(char* line, const char* word) {
    int count = 0;
    int word_len = strlen(word);

    char *ptr = strstr(line, word);

    while(ptr != NULL) {
        count++;

        ptr = strstr(ptr + word_len, word);
    }

    return count;
}

int search_into_file(const char* file_path, const char* target_word) {
    FILE* file = fopen(file_path, "r");

    if(file == NULL) {
        printf("Error: Could not open file %s\n", file_path);
        return 0;
    }

    char line_buffer[65536];
    int file_matches = 0;
    while(fgets(line_buffer, sizeof(line_buffer), file)) {
        file_matches += count_words_in_line(line_buffer, target_word);
    }

    fclose(file);
    return file_matches;
}

void* worker_function(void* args) {
    ThreadArgs* shared_data = (ThreadArgs*) args;
    
    while(1) {
        char *path = dequeue(shared_data->queue);

        if(strcmp(path, "STOP") == 0 || path == NULL) {
            if(path != NULL) free(path);
            break;
        }

        int local_matches = search_into_file(path, shared_data->target_word);

        pthread_mutex_lock(&shared_data->matches_lock);
        total_matches += local_matches;
        pthread_mutex_unlock(&shared_data->matches_lock);

        printf("[Thread %ld] processed %s (Found: %d)\n", pthread_self(), path, local_matches);
        free(path);
    }

    pthread_exit(NULL);

}

int main() {

    printf("Creating a Queue\n");
    Queue* queue = create_queue();


    pthread_t worker[number_of_thread];

    ThreadArgs shared_data;
    shared_data.queue = queue;
    shared_data.total_matches = &total_matches;
    
    shared_data.target_word = strdup(buffer);

    pthread_mutex_init(&shared_data.matches_lock, NULL);

    for(int i = 0;i < number_of_thread;i++) {
        pthread_create(&worker[i], NULL, worker_function, &shared_data);
    }

    for(int i = 0;i < number_of_thread;i++) {
        enqueue(queue, "STOP");
    }

    for(int i = 0;i < number_of_thread;i++) {
        pthread_join(worker[i], NULL);
    }

    printf("\n-------------------------------------------------\n");
    printf("[Searcher Process %d] Finished. Local Matches: %d\n", 1, total_matches);
    printf("-------------------------------------------------\n");

    free((void*)shared_data.target_word);
    pthread_mutex_destroy(&shared_data.matches_lock);
    free_queue(queue);

    return 0;
}