#include "mongoose.h"
#include <windows.h>
#include <shellapi.h>
#include <unistd.h>
#include "cJSON.h"
#include "linkedlist.h"

#define WM_TRAYICON  (WM_USER + 1)
#define ID_OPEN      1001
#define ID_EXIT      1002
#define NOTIFY_ID    1

typedef struct {
    int idx;
    PROCESS_INFORMATION p;
} Task;

// ========== 全局状态 ==========
static HINSTANCE       g_hInstance;
static HWND            g_hwnd;
static HANDLE          g_hServerThread;
static char root_path[1024 - 256];
char web_path[1024];
// HINSTANCE hDll;
struct mg_mgr g_mgr;
NOTIFYICONDATAW g_nid;
cJSON* root;
LinkedList* tasks = &((LinkedList) { 0, NULL, NULL });
volatile int g_serverRunning = 1;


const char* http_reply_success = "{\"success\":true,\"message\":\"操作成功\"}";
const char* http_reply_failed = "{\"success\":false,\"message\":\"操作失败\"}";

// ========== Mongoose 事件处理 ==========
void fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message* hm = (struct mg_http_message*)ev_data;
    if (mg_match(hm->uri, mg_str("/list"), NULL)) {
        if (root == NULL) return;
        char* str = cJSON_PrintUnformatted(root);
        mg_http_reply(c, 200, "Content-Type: application/json; charset=utf-8\r\n", str);
        free(str);
    }
    else if (mg_match(hm->uri, mg_str("/add"), NULL)) {
        //hm->body.buf {"listen":"TCP6:8989","target":"TCP4:5500","host":"127.0.0.1","status":"inactive"}
        cJSON* item = cJSON_Parse(hm->body.buf);
        if (cJSON_AddItemToArray(root, item) != 0) mg_http_reply(c, 200, "", http_reply_success);
        else mg_http_reply(c, 200, "", http_reply_failed);
    }
    else if (mg_match(hm->uri, mg_str("/delete"), NULL)) {
        //hm->body.buf {"id":3}
        cJSON* json = cJSON_Parse(hm->body.buf);
        if (json != NULL) {
            cJSON* item = cJSON_GetObjectItemCaseSensitive(json, "id");
            int id = item->valueint;
            if (id < cJSON_GetArraySize(root)) cJSON_DeleteItemFromArray(root, id);
            mg_http_reply(c, 200, "", http_reply_success);
            cJSON_Delete(json);
        }
        else {
            mg_http_reply(c, 200, "", http_reply_failed);
        }
    }
    else if (mg_match(hm->uri, mg_str("/toggle"), NULL)) {
        //hm->body.buf {"id":1,"status":"active"}
        cJSON* json = cJSON_Parse(hm->body.buf);
        if (json == NULL) {
            mg_http_reply(c, 200, "", http_reply_failed);
            cJSON_Delete(json);
            return;
        }
        cJSON* item = cJSON_GetObjectItemCaseSensitive(json, "id");
        if (item == NULL) {
            mg_http_reply(c, 200, "", http_reply_failed);
            cJSON_Delete(json);
            return;
        }
        int id = item->valueint;
        if (id >= cJSON_GetArraySize(root)) {
            mg_http_reply(c, 200, "", http_reply_failed);
            cJSON_Delete(json);
            return;
        }
        cJSON* data = cJSON_GetArrayItem(root, id);
        //data {"listen":"TCP6:8989","target":"TCP4:5500","host":"127.0.0.1","status":"inactive"}
        cJSON* status = cJSON_GetObjectItemCaseSensitive(data, "status");
        if (strcmp(status->valuestring, "active") == 0) {
            LinkedList* current = tasks->next;
            while (current)
            {
                Task* task = (Task*)current->data;
                if (task->idx == id) {
                    TerminateProcess(task->p.hProcess, 1);
                    CloseHandle(task->p.hProcess);
                    CloseHandle(task->p.hThread);
                    LinkedListRemove(tasks, current->idx);
                    cJSON* new_item = cJSON_CreateString("inactive");
                    cJSON_ReplaceItemInObject(data, "status", new_item);
                    mg_http_reply(c, 200, "", http_reply_success);
                    cJSON_Delete(json);
                    return;
                }
                current = current->next;
            }
            mg_http_reply(c, 200, "", http_reply_failed);
            cJSON_Delete(json);
            return;
        }
        Task* task = (Task*)malloc(sizeof(Task));
        if (task == NULL) {
            mg_http_reply(c, 200, "", "{\"success\":false,\"message\":\"内存不足\"}");
            cJSON_Delete(json);
            return;
        }
        cJSON* lis = cJSON_GetObjectItemCaseSensitive(data, "listen");
        cJSON* tar = cJSON_GetObjectItemCaseSensitive(data, "target");
        cJSON* host = cJSON_GetObjectItemCaseSensitive(data, "host");
        char lisproc[8], tarproc[8], tarhost[128];
        int lisport, tarport;
        sscanf(lis->valuestring, "%[^:]:%d", lisproc, &lisport);
        sscanf(tar->valuestring, "%[^:]:%d", tarproc, &tarport);
        if (strstr(tarproc, "6") != NULL) sprintf(tarhost, "[%s]", host->valuestring);
        sprintf(tarhost, "%s", host->valuestring);
        char cmd[1024];
        if (strstr(lis->valuestring, "TCP"))
            sprintf(cmd, "%s\\bin\\socat.exe %s-LISTEN:%d,reuseaddr,fork %s:%s:%d",
                root_path, lisproc, lisport, tarproc, tarhost, tarport);
        else
            sprintf(cmd, "%s\\bin\\socat.exe %s-RECVFROM:%d,reuseaddr,fork %s-SENDTO:%s:%d",
                root_path, lisproc, lisport, tarproc, tarhost, tarport);
        STARTUPINFOA si = { sizeof(si) };
        if (!CreateProcessA(NULL, cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &si, &task->p)) {
            free(task);
            mg_http_reply(c, 200, "", "{\"success\":false,\"message\":\"执行任务失败\"}");
            cJSON_Delete(json);
            return;
        }
        task->idx = id;
        LinkedListAdd(tasks, task);
        cJSON* new_item = cJSON_CreateString("active");
        cJSON_ReplaceItemInObject(data, "status", new_item);
        mg_http_reply(c, 200, "", http_reply_success);
        cJSON_Delete(json);
    }
    else {
        struct mg_http_serve_opts opts = { .root_dir = web_path };
        mg_http_serve_dir(c, hm, &opts);
    }
}

// ========== 服务器线程 ==========
static DWORD WINAPI ServerThread(LPVOID param) {
    mg_mgr_init(&g_mgr);
    if (mg_http_listen(&g_mgr, "http://127.0.0.1:8033", fn, NULL) == NULL) return MessageBoxW(NULL, L"端口绑定失败！", L"错误", MB_ICONERROR);
    MessageBoxW(NULL, L"启动成功，可在托盘退出", L"提示", MB_OK);
    ShellExecuteW(NULL, L"open", L"http://localhost:8033/", NULL, NULL, SW_SHOW);
    while (g_serverRunning) mg_mgr_poll(&g_mgr, 1000);
    mg_mgr_free(&g_mgr);
    return 0;
}

// ========== 托盘菜单 ==========
static void ShowTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_OPEN, L"主页");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// ========== 窗口过程 ==========
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)
            ShowTrayMenu(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_OPEN:
            ShellExecuteW(NULL, L"open", L"http://localhost:8033/", NULL, NULL, SW_SHOW);
            break;
        case ID_EXIT:
            g_serverRunning = 0;
            mg_wakeup(&g_mgr, 0, NULL, 0);

            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

char* read_file_to_string(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("无法打开文件");
        return NULL;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 分配内存（文件大小 + 1字节用于字符串结束符）
    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("内存分配失败");
        fclose(file);
        return NULL;
    }
    
    // 读取文件内容
    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';  // 添加字符串结束符

    fclose(file);
    return buffer;
}

int write_string_to_file(const char* filename, const char* content) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("无法打开文件");
        return -1;
    }

    int result = fprintf(file, "%s", content);
    fclose(file);

    if (result < 0) {
        perror("写入失败");
        return -1;
    }

    return 0;  // 成功
}

// ========== 入口 ==========
int main(void) {

    if (getcwd(root_path, sizeof(root_path)) == NULL)
        return MessageBoxW(NULL, L"找不到程序目录", L"错误", MB_ICONERROR);
    sprintf(web_path, "%s\\web_root", root_path);

    // 加载数据
    const char* data_file = "web_root/data.json";
    char* str = read_file_to_string(data_file);
    root = cJSON_Parse(str);    // 解析数据
    free(str);
    g_hInstance = GetModuleHandleW(NULL);
    WNDCLASSW wc = { .lpfnWndProc = WndProc, .hInstance = g_hInstance,
                     .lpszClassName = L"TrayServerWnd" };
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(0, L"TrayServerWnd", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, g_hInstance, NULL);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = NOTIFY_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpyW(g_nid.szTip, L"流量转发");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_hServerThread = CreateThread(NULL, 0, ServerThread, NULL, 0, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 关闭服务器进程
    WaitForSingleObject(g_hServerThread, 5000);
    CloseHandle(g_hServerThread);
    // 关闭 socat 进程
    tasks = tasks->next;
    while (tasks)
    {
        Task* task = (Task*)tasks->data;
        TerminateProcess(task->p.hProcess, 1);
        CloseHandle(task->p.hProcess);
        CloseHandle(task->p.hThread);
        free(tasks->data);
        free(tasks);
        tasks = tasks->next;
    }
    // 保存数据至文件
    cJSON* item;
    cJSON_ArrayForEach(item, root){
        cJSON* new_item = cJSON_CreateString("inactive");
        cJSON_ReplaceItemInObject(item, "status", new_item);
    }
    str = cJSON_Print(root);
    write_string_to_file(data_file, str);
    free(str);
    // 释放堆内存
    cJSON_Delete(root);
    return 0;
}