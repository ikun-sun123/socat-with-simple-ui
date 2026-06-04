#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

typedef struct LinkedList {
    int idx;        // 头节点：记录链表节点个数（最大索引+1）；普通节点：存储节点索引
    void* data;
    struct LinkedList* next;
} LinkedList;


// 追加节点到链表末尾
void LinkedListAdd(LinkedList* list, void* data);

// 根据索引删除节点
void LinkedListRemove(LinkedList* list, int idx);

// 释放全部节点及其data指针指向的地址
void LinkedListFree(LinkedList* list);

// 遍历链表，并回调fn函数，每次将此次节点传递给fn
// 当fn返回0时继续遍历，返回1时停止遍历
void LinkedListTraverse(LinkedList* list, int (*fn)(LinkedList* node, va_list args), ...);