#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <windows.h>
#include "libs/linkedlist.h"
#include "libs/cJSON.h"

// 如果标准 asprintf 不可用
#ifndef asprintf
    // 安全的自实现回退
static int safe_asprintf(char** strp, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) return -1;
    *strp = malloc(len + 1);
    if (!*strp) return -1;
    va_start(ap, fmt);
    vsnprintf(*strp, len + 1, fmt, ap);
    va_end(ap);
    return len;
}
#define asprintf safe_asprintf
#endif

typedef struct {
    char protocol[8];   //协议
    int listenPort; // 监听端口
    char listenNet[8];  // 监听网络
    int forwardPort;    // 目标端口
    char targetNet[8];  // 目标网络
    char targetHost[64];    // 目标主机
    char status[16];    // 状态
} Rule;

typedef struct {
    int id;
    PROCESS_INFORMATION pi;
} Task;

LinkedList* tasks = &((LinkedList) { 0, NULL, NULL });

#ifdef __cplusplus
extern "C" {
#endif

    // 函数声明
    __declspec(dllexport) int sum(LinkedList* list, char* params, char** out);
    __declspec(dllexport) int list(LinkedList* list, char* params, char** out);
    __declspec(dllexport) int add(LinkedList* list, char* params, char** out);
    __declspec(dllexport) int update(LinkedList* list, char* params, char** out);
    __declspec(dllexport) int delete(LinkedList* list, char* params, char** out);
    __declspec(dllexport) int option(LinkedList* list, char* params, char** out);
    __declspec(dllexport) void destroyAllTask();

#ifdef __cplusplus
}
#endif


int sum(LinkedList* list, char* params, char** out) {
    asprintf(out, "成功");
    return 200;
}

int list(LinkedList* list, char* params, char** out) {
    // 创建JSON数组
    cJSON* jsonArray = cJSON_CreateArray();
    if (jsonArray == NULL) return 500;  // 内部错误
    // 遍历所有节点
    LinkedList* currentTask = list->next;  // 跳过头节点
    while (currentTask != NULL) {
        Rule* rule = (Rule*)currentTask->data;
        // 为每个Rule创建JSON对象
        cJSON* jsonRule = cJSON_CreateObject();
        if (jsonRule == NULL) continue;
        // 添加各个字段
        cJSON_AddStringToObject(jsonRule, "protocol", rule->protocol);
        cJSON_AddNumberToObject(jsonRule, "listenPort", rule->listenPort);
        cJSON_AddStringToObject(jsonRule, "listenNet", rule->listenNet);
        cJSON_AddNumberToObject(jsonRule, "forwardPort", rule->forwardPort);
        cJSON_AddStringToObject(jsonRule, "targetNet", rule->targetNet);
        cJSON_AddStringToObject(jsonRule, "targetHost", rule->targetHost);
        cJSON_AddStringToObject(jsonRule, "status", rule->status);
        // 添加到数组
        cJSON_AddItemToArray(jsonArray, jsonRule);
        currentTask = currentTask->next;
    }
    // 将JSON数组转换为字符串
    *out = cJSON_PrintUnformatted(jsonArray);
    if (*out == NULL) {
        cJSON_Delete(jsonArray);
        return 500;  // 转换失败
    }
    // 清理JSON对象
    cJSON_Delete(jsonArray);
    return 200;  // 成功
}

int add(LinkedList* list, char* params, char** out) {
    Rule* rule = (Rule*)malloc(sizeof(Rule));
    cJSON* json = cJSON_Parse(params);
    if (json == NULL) {
        const char* err = cJSON_GetErrorPtr();
        asprintf(out, "{\"error\":\"JSON parse failed near: %s\"}", err ? err : "unknown");
        return 400;
    }
    cJSON* item;
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "protocol")))
        strncpy(rule->protocol, item->valuestring, sizeof(rule->protocol) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "listenPort")))
        rule->listenPort = (int)item->valuedouble;
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "listenNet")))
        strncpy(rule->listenNet, item->valuestring, sizeof(rule->listenNet) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "forwardPort")))
        rule->forwardPort = (int)item->valuedouble;
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "targetNet")))
        strncpy(rule->targetNet, item->valuestring, sizeof(rule->targetNet) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "targetHost")))
        strncpy(rule->targetHost, item->valuestring, sizeof(rule->targetHost) - 1);
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "status")))
        strncpy(rule->status, item->valuestring, sizeof(rule->status) - 1);
    cJSON_Delete(json);
    LinkedListAppend(list, rule);
    printf("%s\n", params);
    asprintf(out, "成功");
    return 200;
}

int updateHandle(LinkedList* node, va_list args) {
    int idx = va_arg(args, int);
    char* params = va_arg(args, char*);
    if (node->idx != idx) return 0;
    Rule* rule = (Rule*)node->data;
    sscanf(strstr(params, "protocol"), "protocol\":\"%[^\"]", rule->protocol);
    sscanf(strstr(params, "listenPort"), "listenPort\":%d", &rule->listenPort);
    sscanf(strstr(params, "listenNet"), "listenNet\":\"%[^\"]", rule->listenNet);
    sscanf(strstr(params, "forwardPort"), "forwardPort\":%d", &rule->forwardPort);
    sscanf(strstr(params, "targetNet"), "targetNet\":\"%[^\"]", rule->targetNet);
    sscanf(strstr(params, "targetHost"), "targetHost\":\"%[^\"]", rule->targetHost);
    return 1;
}

int update(LinkedList* list, char* params, char** out) {
    int idx = 0;
    sscanf(strstr(params, "idx"), "idx\":%d", &idx);
    LinkedListTraverse(list, updateHandle, idx, params);
    asprintf(out, "成功");
    return 200;
}

int delete(LinkedList* list, char* params, char** out) {
    int idx = 0;
    sscanf(strstr(params, "idx"), "idx\":%d", &idx);
    LinkedListRemove(list, idx);
    asprintf(out, "成功");
    return 200;
}

int optionHandle(LinkedList* node, va_list args) {
    int idx = va_arg(args, int);
    if (node->idx != idx) return 0;
    LinkedList* currentTask = tasks->next;
    while (currentTask)
    {
        Task* task = (Task*)currentTask->data;
        if (task->id == idx) {
            // 停止线程，删除线程，修改状态为“未启动”
            TerminateProcess(task->pi.hProcess, 1);
            CloseHandle(task->pi.hProcess);
            CloseHandle(task->pi.hThread);
            LinkedListRemove(tasks, currentTask->idx);
            strcpy(((Rule*)(node->data))->status, "未启动");
            return 1;
        }
        currentTask = currentTask->next;
    }
    if (currentTask == NULL) {
        // 组装命令，开启线程，保存到tasks链表，修改状态为“运行中”
        Task* task = (Task*)malloc(sizeof(Task));
        task->id = idx;
        char socatPath[512];
        char cmd[1024];
        Rule* rule = (Rule*)node->data;
        int net1, net2;
        getcwd(socatPath, sizeof(socatPath));
        sscanf(rule->listenNet, "IPv%d", &net1);
        sscanf(rule->targetNet, "IPv%d", &net2);
        if (strcmp(rule->protocol, "TCP") == 0) {
            if (net2 == 6)
                sprintf(cmd, "%s\\bin\\socat TCP%d-LISTEN:%d,reuseaddr,fork TCP%d:[%s]:%d", socatPath, net1, rule->listenPort, net2, rule->targetHost, rule->forwardPort);
            else
                sprintf(cmd, "%s\\bin\\socat TCP%d-LISTEN:%d,reuseaddr,fork TCP%d:%s:%d", socatPath, net1, rule->listenPort, net2, rule->targetHost, rule->forwardPort);
        }
        else {
            if (net2 == 6)
                sprintf(cmd, "%s\\bin\\socat UDP%d-RECVFROM:%d,reuseaddr,fork UDP%d-SENDTO:[%s]:%d", socatPath, net1, rule->listenPort, net2, rule->targetHost, rule->forwardPort);
            else
                sprintf(cmd, "%s\\bin\\socat UDP%d-RECVFROM:%d,reuseaddr,fork UDP%d-SENDTO:%s:%d", socatPath, net1, rule->listenPort, net2, rule->targetHost, rule->forwardPort);
        }
        STARTUPINFOA si = { sizeof(si) };
        if (!CreateProcessA(NULL, cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &si, &task->pi)) {
            free(task);
            return 1;
        }
        LinkedListAppend(tasks, task);
        strcpy(rule->status, "运行中");
    }
    return 1;
}

int option(LinkedList* list, char* params, char** out) {
    int idx = 0;
    sscanf(strstr(params, "idx"), "idx\":%d", &idx);
    LinkedListTraverse(list, optionHandle, idx);
    asprintf(out, "成功");
    return 200;
}

void destroyAllTask() {
    LinkedList* current = tasks->next;
    while (current)
    {
        Task* task = (Task*)current->data;
        TerminateProcess(task->pi.hProcess, 1);
        CloseHandle(task->pi.hProcess);
        CloseHandle(task->pi.hThread);
        current = current->next;
    }
    LinkedListFree(tasks);
}
