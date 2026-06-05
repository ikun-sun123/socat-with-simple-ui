#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

typedef struct LinkedList {
    int idx;        // 头节点：记录链表节点个数；普通节点：存储节点索引
    void* data;
    struct LinkedList* next;
} LinkedList;


// 追加节点到链表末尾
void LinkedListAdd(LinkedList* list, void* data);

// 根据索引删除节点
void LinkedListRemove(LinkedList* list, int idx);

// 释放全部节点及其data指针指向的地址
void LinkedListFree(LinkedList* list);

// 请勿在循环中free或者改变item->next指针
#define LinkedListForEach(item, list)\
    for(item = (list != NULL) ? (list)->next : NULL; item != NULL; item = (item)->next)
