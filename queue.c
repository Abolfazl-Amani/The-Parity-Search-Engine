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
        printf("Memory Allocation Encountered with an Error!\n");
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
        printf("Memory Allocation Encountered with an Error!\n");
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

char* dequeue(Queue* q) {
    if(empty(q)){
        printf("Can not Modify Queue Because Queue is Empty!\n");
        return NULL;
    }
    Node* temp_node = q->head;
    if(q->size == 1) {
        q->head = q->tail = NULL;
    }else {
        q->head = q->head->next;
    }
    
    q->size--;
    char* file_path = temp_node->file_path;

    free(temp_node);

    return file_path;

}

int main() {

    printf("Creating a Queue\n");
    Queue* queue = create_queue();

    printf("Size: %d\n", queue->size);

    printf("Add an Element into Queue\n");
    enqueue(queue, "Abolfazl-Amani");

    printf("Add an Element into Queue\n");
    enqueue(queue, "Hasan-Amani");

    printf("Remove an Element in Queue\n");
    dequeue(queue);

    printf("Size: %d\n", queue->size);
    return 0;
}