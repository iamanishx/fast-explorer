#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string.h>
#include <stdio.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define ID_TREE_VIEW 1001
#define ID_LIST_VIEW 1002
#define ID_STATUS_BAR 1003
#define ID_TOOLBAR 1004
#define IDM_UP 2001
#define IDM_REFRESH 2002
#define IDM_BACK 2003
#define IDM_FORWARD 2004

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HWND hTree, hList, hStatus, hToolBar;
HIMAGELIST hImageList;
char currentPath[MAX_PATH];
char history[50][MAX_PATH];
int historyPos = -1;
int historyCount = 0;
int isNavigating = 0;

void PopulateTreeView();
void PopulateListView(const char* path);
void NavigateUp();
void NavigateBack();
void NavigateForward();
void Refresh();
void OpenSelectedItem();
void ResizeControls(HWND hwnd);
void AddToHistory(const char* path);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    InitCommonControls();

    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "FastExplorer";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(0, "FastExplorer", "Fast Explorer", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);

    HWND hBtnBack = CreateWindow("BUTTON", "< Back", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 5, 70, 25, hwnd, (HMENU)IDM_BACK, hInstance, NULL);
    HWND hBtnForward = CreateWindow("BUTTON", "> Fwd", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 85, 5, 60, 25, hwnd, (HMENU)IDM_FORWARD, hInstance, NULL);
    HWND hBtnUp = CreateWindow("BUTTON", "^ Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 5, 60, 25, hwnd, (HMENU)IDM_UP, hInstance, NULL);
    HWND hBtnRefresh = CreateWindow("BUTTON", "Rfrsh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 215, 5, 60, 25, hwnd, (HMENU)IDM_REFRESH, hInstance, NULL);
    hToolBar = hBtnBack;

    hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)ID_STATUS_BAR, hInstance, NULL);

    hTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL, WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
        0, 0, 0, 0, hwnd, (HMENU)ID_TREE_VIEW, hInstance, NULL);

    hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hwnd, (HMENU)ID_LIST_VIEW, hInstance, NULL);

    LVCOLUMN lvc;
    memset(&lvc, 0, sizeof(lvc));
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 280;
    lvc.pszText = "Name";
    ListView_InsertColumn(hList, 0, &lvc);
    lvc.cx = 90;
    lvc.pszText = "Size";
    ListView_InsertColumn(hList, 1, &lvc);
    lvc.cx = 120;
    lvc.pszText = "Type";
    ListView_InsertColumn(hList, 2, &lvc);
    lvc.cx = 140;
    lvc.pszText = "Modified";
    ListView_InsertColumn(hList, 3, &lvc);

    hImageList = ImageList_Create(16, 16, ILC_COLOR32, 16, 16);
    TreeView_SetImageList(hTree, hImageList, TVSIL_NORMAL);
    ListView_SetImageList(hList, hImageList, LVSIL_SMALL);

    SHFILEINFO shfi;
    SHGetFileInfo("C:\\", FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);

    PopulateTreeView();
    ResizeControls(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void PopulateTreeView() {
    TreeView_DeleteAllItems(hTree);
    char drives[256] = {0};
    GetLogicalDriveStrings(255, drives);
    char* d = drives;
    while (*d) {
        TVINSERTSTRUCT tvis;
        memset(&tvis, 0, sizeof(tvis));
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_SORT;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
        tvis.item.pszText = d;
        tvis.item.iImage = 0;
        tvis.item.iSelectedImage = 0;
        tvis.item.cChildren = 1;
        HTREEITEM hItem = TreeView_InsertItem(hTree, &tvis);
        
        TVINSERTSTRUCT tvisD;
        memset(&tvisD, 0, sizeof(tvisD));
        tvisD.hParent = hItem;
        tvisD.hInsertAfter = TVI_FIRST;
        tvisD.item.mask = TVIF_TEXT;
        tvisD.item.pszText = "";
        TreeView_InsertItem(hTree, &tvisD);
        d += strlen(d) + 1;
    }
}

void PopulateListView(const char* path) {
    strncpy(currentPath, path, MAX_PATH - 1);
    currentPath[MAX_PATH - 1] = 0;
    if (!isNavigating) AddToHistory(path);
    ListView_DeleteAllItems(hList);
    char sp[MAX_PATH];
    sprintf(sp, "%s\\*", path);
    WIN32_FIND_DATA fd;
    HANDLE hf = FindFirstFile(sp, &fd);
    if (hf == INVALID_HANDLE_VALUE) { SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)"0 items"); return; }

    int idx = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        int isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        LVITEM lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT | LVIF_IMAGE;
        lvi.iItem = idx;
        lvi.pszText = fd.cFileName;
        lvi.iImage = isDir ? 0 : 1;
        int row = ListView_InsertItem(hList, &lvi);

        if (!isDir) {
            ULARGE_INTEGER fs;
            fs.LowPart = fd.nFileSizeLow;
            fs.HighPart = fd.nFileSizeHigh;
            char sz[32];
            if (fs.QuadPart < 1024) sprintf(sz, "%llu B", fs.QuadPart);
            else if (fs.QuadPart < 1048576) sprintf(sz, "%llu KB", fs.QuadPart / 1024);
            else if (fs.QuadPart < 1073741824) sprintf(sz, "%.1f MB", fs.QuadPart / 1048576.0);
            else sprintf(sz, "%.2f GB", fs.QuadPart / 1073741824.0);
            ListView_SetItemText(hList, row, 1, sz);
        } else {
            ListView_SetItemText(hList, row, 2, "File folder");
        }

        FILETIME lft;
        FileTimeToLocalFileTime(&fd.ftLastWriteTime, &lft);
        SYSTEMTIME st;
        FileTimeToSystemTime(&lft, &st);
        char dt[64];
        sprintf(dt, "%02d/%02d/%04d %02d:%02d", st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute);
        ListView_SetItemText(hList, row, 3, dt);
        idx++;
    } while (FindNextFile(hf, &fd));
    FindClose(hf);

    char status[64];
    sprintf(status, "%d items", idx);
    SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)status);

    SendMessage(hToolBar, TB_SETSTATE, IDM_BACK, (historyPos > 0 ? 0x01 : 0x00));
    SendMessage(hToolBar, TB_SETSTATE, IDM_FORWARD, (historyPos < historyCount - 1 ? 0x01 : 0x00));
}

void NavigateUp() {
    if (strlen(currentPath) <= 3) return;
    char* p = strrchr(currentPath, '\\');
    if (p && p > currentPath + 2) {
        *p = 0;
        AddToHistory(currentPath);
        PopulateListView(currentPath);
    }
}

void NavigateBack() {
    if (historyPos > 0) {
        historyPos--;
        isNavigating = 1;
        PopulateListView(history[historyPos]);
        isNavigating = 0;
    }
}

void NavigateForward() {
    if (historyPos < historyCount - 1) {
        historyPos++;
        isNavigating = 1;
        PopulateListView(history[historyPos]);
        isNavigating = 0;
    }
}

void AddToHistory(const char* path) {
    if (historyPos < historyCount - 1) {
        historyCount = historyPos + 1;
    }
    if (historyCount < 50) {
        strncpy(history[historyCount], path, MAX_PATH - 1);
        history[historyCount][MAX_PATH - 1] = 0;
        historyCount++;
        historyPos = historyCount - 1;
    }
}

void Refresh() { if (strlen(currentPath) > 0) PopulateListView(currentPath); }

void OpenSelectedItem() {
    int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (sel == -1) return;
    char name[MAX_PATH];
    LVITEM lvi;
    memset(&lvi, 0, sizeof(lvi));
    lvi.mask = LVIF_TEXT;
    lvi.iItem = sel;
    lvi.pszText = name;
    lvi.cchTextMax = MAX_PATH;
    ListView_GetItem(hList, &lvi);
    char fp[MAX_PATH];
    sprintf(fp, "%s\\%s", currentPath, name);
    WIN32_FIND_DATA fd;
    HANDLE hf = FindFirstFile(fp, &fd);
    if (hf != INVALID_HANDLE_VALUE) {
        FindClose(hf);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) PopulateListView(fp);
        else ShellExecute(NULL, "open", fp, NULL, NULL, SW_SHOWNORMAL);
    }
}

void ResizeControls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    RECT rs;
    GetWindowRect(hStatus, &rs);
    int th = 35;
    int sh = rs.bottom - rs.top;
    int tw = 250;
    SetWindowPos(hTree, NULL, 0, th, tw, rc.bottom - th - sh, SWP_NOZORDER);
    SetWindowPos(hList, NULL, tw, th, rc.right - tw, rc.bottom - th - sh, SWP_NOZORDER);
    SendMessage(hStatus, WM_SIZE, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: ResizeControls(hwnd); return 0;
        case WM_COMMAND:
            if (wp == IDM_BACK) NavigateBack();
            else if (wp == IDM_FORWARD) NavigateForward();
            else if (wp == IDM_UP) NavigateUp();
            else if (wp == IDM_REFRESH) Refresh();
            return 0;
        case WM_NOTIFY: {
            NMHDR* pnm = (NMHDR*)lp;
            if (pnm->idFrom == ID_TREE_VIEW && pnm->code == TVN_SELCHANGED) {
                HTREEITEM hSel = TreeView_GetSelection(hTree);
                if (hSel) {
                    char buf[MAX_PATH];
                    TVITEM tvi;
                    memset(&tvi, 0, sizeof(tvi));
                    tvi.mask = TVIF_TEXT;
                    tvi.hItem = hSel;
                    tvi.pszText = buf;
                    tvi.cchTextMax = MAX_PATH;
                    TreeView_GetItem(hTree, &tvi);
                    char path[MAX_PATH];
                    strcpy(path, buf);
                    HTREEITEM hp = TreeView_GetParent(hTree, hSel);
                    while (hp) {
                        char pb[MAX_PATH];
                        TVITEM pt;
                        memset(&pt, 0, sizeof(pt));
                        pt.mask = TVIF_TEXT;
                        pt.hItem = hp;
                        pt.pszText = pb;
                        pt.cchTextMax = MAX_PATH;
                        TreeView_GetItem(hTree, &pt);
                        sprintf(path, "%s\\%s", pb, path);
                        hp = TreeView_GetParent(hTree, hp);
                    }
                    PopulateListView(path);
                }
            } else if (pnm->idFrom == ID_LIST_VIEW && pnm->code == NM_DBLCLK) {
                OpenSelectedItem();
            }
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}