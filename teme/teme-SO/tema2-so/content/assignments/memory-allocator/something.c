#include "stdio.h"
#include "stdlib.h"
#define MMAP_THRESHOLD (128 * 1024)
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

struct node {
    int value;
    struct node *next;
};
int main(void)
{
    struct node *node1 = malloc(sizeof(struct node));
    node1->value = 5;
    node1->next = NULL;
    
    struct node *node2 = malloc(sizeof(struct node));;
    node2->value = 10;
    node2->next = node1;

    struct node *node3 = malloc(sizeof(struct node));;
    node3->value = 15;
    node3->next = node2;

    struct node *current = node3;
    struct node *prev = NULL;
    while (current != NULL) {
        printf("%d\n", current->value);
    

        current = current->next;
    }

    return 0;
}