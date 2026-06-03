#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct LinkedList {
    int idx;        // 头节点：记录链表节点个数（最大索引+1）；普通节点：存储节点索引
    void* data;
    struct LinkedList* next;
} LinkedList;

// 初始化链表头节点（头节点idx记录节点个数，初始为0）
void LinkedListInit(LinkedList* list) {
    if (list == NULL) return;
    list->idx = 0;      // 节点个数为0
    list->data = NULL;
    list->next = NULL;
}

// 追加节点到链表末尾
void LinkedListAppend(LinkedList* list, void* data) {
    if (list == NULL || data == NULL) return;

    // 找到最后一个节点
    LinkedList* current = list;
    while (current->next != NULL) {
        current = current->next;
    }

    // 创建新节点
    LinkedList* newNode = (LinkedList*)malloc(sizeof(LinkedList));
    if (newNode == NULL) return;

    // 新节点的索引 = 当前节点个数（头节点的idx）
    int newIdx = list->idx;
    newNode->idx = newIdx;
    newNode->data = data;
    newNode->next = NULL;

    current->next = newNode;

    // 头节点记录数+1
    list->idx++;
}

// 根据索引删除节点（索引从0开始）
void LinkedListRemove(LinkedList* list, int idx) {
    if (list == NULL || list->next == NULL || idx < 0 || idx >= list->idx) return;

    LinkedList* prev = list;
    LinkedList* current = list->next;

    while (current != NULL) {
        if (current->idx == idx) {
            // 找到目标节点，断开链接
            prev->next = current->next;
            // 释放节点内存
            free(current->data);
            free(current);

            // 更新后面所有节点的idx（索引减1）
            LinkedList* temp = prev->next;
            int newIdx = idx;
            while (temp != NULL) {
                temp->idx = newIdx++;
                temp = temp->next;
            }

            // 头节点记录数减1
            list->idx--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

// 释放全部节点及其data指针指向的地址
void LinkedListFree(LinkedList* list) {
    if (list == NULL) return;

    LinkedList* current = list->next;
    LinkedList* nextNode;

    // 释放所有数据节点
    while (current != NULL) {
        nextNode = current->next;
        if (current->data != NULL) {
            free(current->data);   // 释放data指向的内存
        }
        free(current);             // 释放节点本身
        current = nextNode;
    }

    // 重置头节点
    list->next = NULL;
    list->idx = 0;      // 节点个数归零
    list->data = NULL;
}

// 遍历链表，并回调fn函数，每次将此次节点传递给fn
// 当fn返回0时继续遍历，返回1时停止遍历
void LinkedListTraverse(LinkedList* list, int (*fn)(LinkedList* node, va_list args), ...) {
    if (list == NULL || fn == NULL) return;

    LinkedList* current = list->next;  // 跳过头节点

    va_list args;
    va_start(args, fn);

    while (current != NULL) {
        if (fn(current, args) == 1) break;
        current = current->next;
    }

    va_end(args);
}