#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    char* file_path;
    Node* next;
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
        return 1;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

int main() {


    return 0;
}