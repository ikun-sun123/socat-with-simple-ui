#include "mongoose.h"
#include <windows.h>
#include <shellapi.h>
#include <unistd.h>
#include "linkedlist.h"

#define WM_TRAYICON  (WM_USER + 1)
#define ID_OPEN      1001
#define ID_EXIT      1002
#define NOTIFY_ID    1

typedef struct {
    char protocol[8];   //协议
    int listenPort; // 监听端口
    char listenNet[8];  // 监听网络
    int forwardPort;    // 目标端口
    char targetNet[8];  // 目标网络
    char targetHost[64];    // 目标主机
    char status[16];    // 状态
} Rule;

// ========== 全局状态 ==========
static HINSTANCE       g_hInstance;
static HWND            g_hwnd;
static HANDLE          g_hServerThread;
static char web_root_path[128];
HINSTANCE hDll;
LinkedList* rules;
struct mg_mgr g_mgr;
NOTIFYICONDATAW g_nid;
volatile int g_serverRunning = 1;

// ========== Mongoose 事件处理 ==========
static void fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        char method[64] = { 0 }, params[256] = { 0 };
        sscanf(hm->uri.buf, "/%[^? ]", method);
        if (mg_strcmp(hm->method, mg_str("GET")) == 0) { // GET 请求
            if (hm->query.len > 0) sscanf(hm->query.buf, "%[^ ]", params);
        }
        else { // POST 等请求
            if (hm->body.len > 0) sscanf(hm->body.buf, "%[^ ]", params);
        }
        FARPROC cb = GetProcAddress(hDll, method);
        if (cb != NULL) {
            char* result = NULL;
            int code = cb(rules, params, &result);
            mg_http_reply(c, code, "Content-Type: text/plain; charset=utf-8\r\n", result);
            free(result);
        }
        else {
            struct mg_http_serve_opts opts = { .root_dir = web_root_path };
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

// ========== 服务器线程 ==========
static DWORD WINAPI ServerThread(LPVOID param) {
    mg_mgr_init(&g_mgr);
    if (mg_http_listen(&g_mgr, "http://127.0.0.1:8080", fn, NULL) == NULL) return MessageBoxW(NULL, L"端口绑定失败！", L"错误", MB_ICONERROR);
    MessageBoxW(NULL, L"启动成功，可在托盘退出", L"提示", MB_OK);
    ShellExecuteW(NULL, L"open", L"http://localhost:8080/", NULL, NULL, SW_SHOW);
    while (g_serverRunning) mg_mgr_poll(&g_mgr, 1000);
    mg_mgr_free(&g_mgr);
    return 0;
}

// 保存至 data.csv 文件
int saveRulesToFile(LinkedList* node, va_list args) {
    FILE* fp = va_arg(args, FILE*);
    Rule* rule = (Rule*)node->data;
    fprintf(fp, "%s,%d,%s,%d,%s,%s\n",
        rule->protocol, rule->listenPort, rule->listenNet,
        rule->forwardPort, rule->targetNet, rule->targetHost);
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
            ShellExecuteW(NULL, L"open", L"http://localhost:8080/", NULL, NULL, SW_SHOW);
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

// ========== 入口 ==========
int main(void) {

    hDll = LoadLibrary("api.dll");
    if (hDll == NULL) return MessageBoxW(NULL, L"共享库api.dll加载失败！", L"错误", MB_ICONERROR);

    if (getcwd(web_root_path, sizeof(web_root_path)) == NULL)
        return MessageBoxW(NULL, L"没有找到web_root目录", L"错误", MB_ICONERROR);
    strcat(web_root_path, "\\web_root");
    // 加载数据
    rules = &((LinkedList) { 0, NULL, NULL });
    FILE* fp = fopen("web_root/data.csv", "r");
    if (fp == NULL) return MessageBoxW(NULL, L"打开数据文件失败", L"错误", MB_ICONERROR);
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) < 5) continue;  // 排除空行
        Rule* rule = (Rule*)malloc(sizeof(Rule));
        sscanf(line, "%[^,],%d,%[^,],%d,%[^,],%[^\n]",
            rule->protocol, &(rule->listenPort), rule->listenNet,
            &(rule->forwardPort), rule->targetNet, rule->targetHost);
        strcpy(rule->status, "未启动");
        LinkedListAppend(rules, rule);
    }
    fclose(fp);

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
    FARPROC cb = GetProcAddress(hDll, "destroyAllTask");
    if (cb != NULL) cb();
    FreeLibrary(hDll);
    // 保存数据至文件
    fp = fopen("web_root/data.csv", "w");
    if (fp != NULL) LinkedListTraverse(rules, saveRulesToFile, fp);
    fclose(fp);

    // 释放链表
    LinkedListFree(rules);
    return 0;
}