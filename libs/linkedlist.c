#include "linkedlist.h"


void LinkedListAdd(LinkedList* list, void* data) {
    if (list == NULL || data == NULL) return;
    LinkedList* current = list;
    while (current->next != NULL) current = current->next;
    LinkedList* newNode = (LinkedList*)malloc(sizeof(LinkedList));
    if (newNode == NULL) return;
    int newIdx = list->idx;
    newNode->idx = newIdx;
    newNode->data = data;
    newNode->next = NULL;
    current->next = newNode;
    list->idx++;
}

void LinkedListRemove(LinkedList* list, int idx) {
    if (list == NULL || list->next == NULL || idx < 0 || idx >= list->idx) return;
    LinkedList* prev = list;
    LinkedList* current = list->next;
    while (current != NULL) {
        if (current->idx == idx) {
            prev->next = current->next;
            free(current->data);
            free(current);
            LinkedList* temp = prev->next;
            int newIdx = idx;
            while (temp != NULL) {
                temp->idx = newIdx++;
                temp = temp->next;
            }
            list->idx--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

void LinkedListFree(LinkedList* list) {
    if (list == NULL) return;
    LinkedList* current = list->next;
    LinkedList* nextNode;
    while (current != NULL) {
        nextNode = current->next;
        if (current->data != NULL) free(current->data);
        free(current);
        current = nextNode;
    }
    list->next = NULL;
    list->idx = 0;
    list->data = NULL;
}

void LinkedListTraverse(LinkedList* list, int (*fn)(LinkedList* node, va_list args), ...) {
    if (list == NULL || fn == NULL) return;
    LinkedList* current = list->next;
    va_list args;
    va_start(args, fn);
    while (current != NULL) {
        if (fn(current, args) == 1) break;
        current = current->next;
    }
    va_end(args);
}