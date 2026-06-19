#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>

int total_matches = 0;

// =====================================
// ====       (Data Structures)    =====  
// =====================================
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

// ==========================================
// ===        (Queue Functions)           ===
// ==========================================
Queue* create_queue() {
    Queue *q = (Queue*) malloc(sizeof(Queue));
    if(q == NULL) {
        printf("Memory Allocation Error!\n");
        return NULL;
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_init(&q->queue_lock, NULL);
    pthread_cond_init(&q->cond_variable, NULL);
    return q;
}

int empty(Queue* q) { return q->size == 0; }

void enqueue(Queue* q, char* path) {
    pthread_mutex_lock(&q->queue_lock);
    Node* new_node = (Node*) malloc(sizeof(Node));
    if(new_node == NULL) {
        pthread_mutex_unlock(&q->queue_lock);
        return;
    }
    new_node->file_path = strdup(path);
    new_node->next = NULL;

    if(empty(q)){
        q->head = q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
    q->size++;
    pthread_mutex_unlock(&q->queue_lock);
    pthread_cond_signal(&q->cond_variable);
}

char* dequeue(Queue* q) {
    pthread_mutex_lock(&q->queue_lock);
    while(empty(q)) {
        pthread_cond_wait(&q->cond_variable, &q->queue_lock);
    }
    Node* temp_node = q->head;
    if(q->size == 1) {
        q->head = q->tail = NULL;
    } else {
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

// ==========================================
// ===            (ProcesSing)            ===
// ==========================================

void traverse_directory(const char *base_path, Queue *queue, int process_id, int total_processes, int *global_file_index) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if ((dir = opendir(base_path)) == NULL) return;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);

        if (stat(path, &statbuf) == -1) continue;

        if (S_ISDIR(statbuf.st_mode)) {
            traverse_directory(path, queue, process_id, total_processes, global_file_index); 
        } else if (S_ISREG(statbuf.st_mode)) {
            if ((*global_file_index) % total_processes == process_id) {
                enqueue(queue, path);
            }
            (*global_file_index)++;
        }
    }
    closedir(dir);
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
    if(file == NULL) return 0;

    char line_buffer[65536];
    int file_matches = 0;
    int line_number = 1;

    while(fgets(line_buffer, sizeof(line_buffer), file)) {
        int line_matches = count_words_in_line(line_buffer, target_word);
        if (line_matches > 0) {
            printf("[Proc: %d | Thread: %ld] File: %s | Line: %d | Count: %d\n",
                   getpid(), pthread_self(), file_path, line_number, line_matches);
            file_matches += line_matches;
        }
        line_number++;
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
        *(shared_data->total_matches) += local_matches;
        pthread_mutex_unlock(&shared_data->matches_lock);

        free(path);
    }
    pthread_exit(NULL);
}

void run_searcher(int j, int num_procs, int num_threads, const char* target_dir, const char* target_word, int write_fd) {
    Queue* queue = create_queue();
    pthread_t worker[num_threads];
    ThreadArgs shared_data;
    shared_data.queue = queue;
    shared_data.total_matches = &total_matches;
    shared_data.target_word = target_word;
    pthread_mutex_init(&shared_data.matches_lock, NULL);

    for(int i = 0; i < num_threads; i++) {
        pthread_create(&worker[i], NULL, worker_function, &shared_data);
    }

    int file_index = 0;
    traverse_directory(target_dir, queue, j, num_procs, &file_index);

    for(int i = 0; i < num_threads; i++) {
        enqueue(queue, "STOP");
    }

    for(int i = 0; i < num_threads; i++) {
        pthread_join(worker[i], NULL);
    }

    int local_matches = total_matches;
    write(write_fd, &local_matches, sizeof(local_matches));

    pthread_mutex_destroy(&shared_data.matches_lock);
    free_queue(queue); 
    exit(0); 
}

// ==========================================
// ===      (Main & Process Monitor)      ===
// ==========================================
int main(int argc, char *argv[]) {
    int num_processes = 0;
    int num_threads = 0;
    char target_word[512] = "";
    char target_dir[1024] = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            num_processes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            strncpy(target_word, argv[++i], sizeof(target_word));
        } else {
            strncpy(target_dir, argv[i], sizeof(target_dir));
        }
    }

    if (num_processes <= 0 || num_threads <= 0 || strlen(target_word) == 0 || strlen(target_dir) == 0) {
        printf("Usage: ./search -p <num_processes> -t <num_threads> -s <search_string> <directory>\n");
        return 1;
    }

    int fd[2];
    if(pipe(fd) < 0) {
        printf("Error: Failed to Create Pipe!\n");
        return 1;
    } 

    printf("======================================================\n");
    printf("Starting Parity Search Engine...\n");
    printf("» Target Word: '%s'\n", target_word);
    printf("» Directory: '%s'\n", target_dir);
    printf("» Processes: %d | Threads per Process: %d\n", num_processes, num_threads);
    printf("======================================================\n\n");

    pid_t child_pids[num_processes];

    for(int j = 0; j < num_processes; j++) {
        pid_t pid = fork();
        if(pid < 0) {
            printf("Error: Failed to Create Child Process!\n");
            return 1;
        } else if(pid == 0) {
            close(fd[0]); 
            run_searcher(j, num_processes, num_threads, target_dir, target_word, fd[1]);
        } else {
            child_pids[j] = pid; 
        }
    }

    int active_children = num_processes;
    int successful_reads = 0;

    while (active_children > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);

        if (pid > 0) {
            int j = -1;
            for (int i = 0; i < num_processes; i++) {
                if (child_pids[i] == pid) { j = i; break; }
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                active_children--;
                successful_reads++;
            } 
            else {
                printf("\n[Process Monitor]  Alert! Process %d (PID: %d) crashed! Restarting...\n", j + 1, pid);
                pid_t new_pid = fork();
                if (new_pid == 0) {
                    close(fd[0]);
                    total_matches = 0; 
                    run_searcher(j, num_processes, num_threads, target_dir, target_word, fd[1]);
                } else {
                    child_pids[j] = new_pid; 
                }
            }
        }
    }

    close(fd[1]); 

    int final_grand_total = 0;
    int received_matches = 0;

    for(int i = 0; i < successful_reads; i++) {
        if(read(fd[0], &received_matches, sizeof(received_matches)) > 0) {
            final_grand_total += received_matches;
        }
    }
    close(fd[0]);

    printf("\n======================================================\n");
    printf("                  FINAL SEARCH REPORT                 \n");
    printf("======================================================\n");
    printf("» GRAND TOTAL MATCHES ACCUMULATED : %d\n", final_grand_total);
    printf("======================================================\n\n");

    return 0;
}