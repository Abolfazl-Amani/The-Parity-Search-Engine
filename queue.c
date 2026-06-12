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

int main() {


    return 0;
}