#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char* file_path;
    struct Node* next;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
    int size;
} Queue;

Queue* create_queue() {
    Queue* queue = (Queue*) malloc(sizeof(Queue));

    if(queue == NULL) {
        printf("Memory Allocation Encountered with an Error!");
        return NULL;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

int empty(Queue* q) {
    return q->size == 0;
}

void enqueue(Queue* q, char* path) {
    Node* new_node = (Node*) malloc(sizeof(Node));

    if(new_node == NULL) {
        printf("Memory Allocation Encountered with an Error!");
        return;
    }

    new_node->file_path = strdup(path);
    new_node->next = NULL;

    if(empty(q)) q->head = q->tail = new_node;
    else {
        q->tail->next = new_node;
        q->tail = new_node;
    }

    q->size++;
}

int main() {

    printf("Creating a Queue\n");
    Queue* queue = create_queue();

    printf("Size: %d\n", queue->size);

    printf("Add an Element into Queue\n");
    enqueue(queue, "Abolfazl-Amani");


    return 0;
}