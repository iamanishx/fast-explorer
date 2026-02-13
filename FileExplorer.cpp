#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif

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

#define OPC_COPY 3001
#define OPC_CUT 3002
#define OPC_PASTE 3003
#define OPC_DELETE 3004
#define OPC_RENAME 3005
#define OPC_OPEN 3006
#define OPC_PROPERTIES 3007
#define OPC_NEWFOLDER 3008
#define OPC_NEWFILE 3009

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HWND hTree, hList, hStatus, hToolBar;
HIMAGELIST hImageList;
char currentPath[MAX_PATH];
char history[50][MAX_PATH];
int historyPos = -1;
int historyCount = 0;
int isNavigating = 0;
char clipPath[MAX_PATH * 100];
int clipOp = 0;
int isCut = 0;

void PopulateTreeView();
void PopulateListView(const char* path);
void NavigateUp();
void NavigateBack();
void NavigateForward();
void Refresh();
void OpenSelectedItem();
void CreateNewFolder();
void CreateNewFile();
void ResizeControls(HWND hwnd);
void AddToHistory(const char* path);
void GetFileIcon(const char* path, SHFILEINFO* shfi);
void CopyFiles();
void CutFiles();
void PasteFiles();
void DeleteFiles();
void DoRename();
void ShowContextMenu(HWND hwnd, int x, int xParam);
void GetSelectedFiles(char* out, int* count);
void AddSystemIcons();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    InitCommonControls();
    clipPath[0] = 0;

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
    DragAcceptFiles(hwnd, TRUE);

    HWND hBtnBack = CreateWindow("BUTTON", "< Back", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 5, 70, 25, hwnd, (HMENU)IDM_BACK, hInstance, NULL);
    HWND hBtnForward = CreateWindow("BUTTON", "> Fwd", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 85, 5, 60, 25, hwnd, (HMENU)IDM_FORWARD, hInstance, NULL);
    HWND hBtnUp = CreateWindow("BUTTON", "^ Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 5, 60, 25, hwnd, (HMENU)IDM_UP, hInstance, NULL);
    HWND hBtnRefresh = CreateWindow("BUTTON", "Rfrsh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 215, 5, 60, 25, hwnd, (HMENU)IDM_REFRESH, hInstance, NULL);
    hToolBar = hBtnBack;

    hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)ID_STATUS_BAR, hInstance, NULL);

    hTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL, WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
        0, 0, 0, 0, hwnd, (HMENU)ID_TREE_VIEW, hInstance, NULL);

    hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_EX_FULLROWSELECT,
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

    AddSystemIcons();
    ListView_SetImageList(hList, hImageList, LVSIL_SMALL);

    PopulateTreeView();
    ResizeControls(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void AddSystemIcons() {
    hImageList = ImageList_Create(16, 16, ILC_COLOR32, 8, 8);
    SHFILEINFO shfi;
    
    SHGetFileInfo("C:\\", FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".txt", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".exe", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".jpg", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".png", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".mp3", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo(".zip", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
    
    SHGetFileInfo("folder", FILE_ATTRIBUTE_DIRECTORY, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    ImageList_AddIcon(hImageList, shfi.hIcon);
    DestroyIcon(shfi.hIcon);
}

int GetIconIndex(const char* name, int isDir) {
    if (isDir) return 0;
    const char* ext = strrchr(name, '.');
    if (!ext) return 1;
    if (!_stricmp(ext, ".exe") || !_stricmp(ext, ".bat") || !_stricmp(ext, ".cmd") || !_stricmp(ext, ".com")) return 2;
    if (!_stricmp(ext, ".jpg") || !_stricmp(ext, ".jpeg") || !_stricmp(ext, ".bmp") || !_stricmp(ext, ".gif")) return 3;
    if (!_stricmp(ext, ".png") || !_stricmp(ext, ".ico") || !_stricmp(ext, ".svg")) return 4;
    if (!_stricmp(ext, ".mp3") || !_stricmp(ext, ".wav") || !_stricmp(ext, ".flac") || !_stricmp(ext, ".aac")) return 5;
    if (!_stricmp(ext, ".zip") || !_stricmp(ext, ".rar") || !_stricmp(ext, ".7z") || !_stricmp(ext, ".tar")) return 6;
    return 1;
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
        lvi.iImage = GetIconIndex(fd.cFileName, isDir);
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
            
            const char* ext = strrchr(fd.cFileName, '.');
            if (ext) {
                char type[64];
                sprintf(type, "%s File", ext + 1);
                strupr(type);
                ListView_SetItemText(hList, row, 2, type);
            }
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

int editItem = -1;

void CreateNewFolder() {
    char name[MAX_PATH] = "New Folder";
    int counter = 1;
    char path[MAX_PATH];
    
    while (1) {
        sprintf(path, "%s\\%s", currentPath, name);
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) break;
        sprintf(name, "New Folder (%d)", counter++);
    }
    
    CreateDirectory(path, NULL);
    editItem = -1;
    Refresh();
    
    int itemCount = ListView_GetItemCount(hList);
    for (int i = 0; i < itemCount; i++) {
        char itemText[MAX_PATH];
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = itemText;
        lvi.cchTextMax = MAX_PATH;
        ListView_GetItem(hList, &lvi);
        if (strcmp(itemText, name) == 0) {
            editItem = i;
            break;
        }
    }
    
    if (editItem >= 0) {
        SetTimer(hList, 2, 50, NULL);
    }
}

void CreateNewFile() {
    char name[MAX_PATH] = "New File.txt";
    int counter = 1;
    char path[MAX_PATH];
    
    while (1) {
        sprintf(path, "%s\\%s", currentPath, name);
        if (GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) break;
        if (counter == 1) sprintf(name, "New File (%d).txt", counter++);
        else sprintf(name, "New File (%d).txt", counter++);
    }
    
    HANDLE hf = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf);
    
    editItem = -1;
    Refresh();
    
    int itemCount = ListView_GetItemCount(hList);
    for (int i = 0; i < itemCount; i++) {
        char itemText[MAX_PATH];
        LVITEM lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = itemText;
        lvi.cchTextMax = MAX_PATH;
        ListView_GetItem(hList, &lvi);
        if (strcmp(itemText, name) == 0) {
            editItem = i;
            break;
        }
    }
    
    if (editItem >= 0) {
        SetTimer(hList, 2, 50, NULL);
    }
}

void OpenSelectedItem() {
    char files[MAX_PATH * 100];
    int count;
    GetSelectedFiles(files, &count);
    if (count == 0) return;
    
    char* p = files;
    while (*p) {
        WIN32_FIND_DATA fd;
        HANDLE hf = FindFirstFile(p, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            FindClose(hf);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                PopulateListView(p);
                return;
            } else {
                ShellExecute(NULL, "open", p, NULL, NULL, SW_SHOWNORMAL);
            }
        }
        p += strlen(p) + 1;
    }
}

void GetSelectedFiles(char* out, int* count) {
    out[0] = 0;
    *count = 0;
    int i = -1;
    while ((i = ListView_GetNextItem(hList, i, LVNI_SELECTED)) != -1) {
        char name[MAX_PATH];
        LVITEM lvi;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = name;
        lvi.cchTextMax = MAX_PATH;
        ListView_GetItem(hList, &lvi);
        char fp[MAX_PATH];
        sprintf(fp, "%s\\%s", currentPath, name);
        strcat(out, fp);
        out += strlen(fp) + 1;
        (*count)++;
    }
    *out = 0;
}

void CopyFiles() {
    char files[MAX_PATH * 100];
    int count;
    GetSelectedFiles(files, &count);
    if (count == 0) return;
    strcpy(clipPath, files);
    clipOp = 1;
    isCut = 0;
}

void CutFiles() {
    char files[MAX_PATH * 100];
    int count;
    GetSelectedFiles(files, &count);
    if (count == 0) return;
    strcpy(clipPath, files);
    clipOp = 1;
    isCut = 1;
}

void PasteFiles() {
    if (!clipOp || !clipPath[0]) return;
    
    char* src = clipPath;
    while (*src) {
        char name[MAX_PATH];
        char* p = strrchr(src, '\\');
        if (p) strcpy(name, p + 1);
        else strcpy(name, src);
        
        char dst[MAX_PATH];
        sprintf(dst, "%s\\%s", currentPath, name);
        
        if (isCut) {
            if (MoveFile(src, dst)) {
            }
        } else {
            if (CopyFile(src, dst, FALSE)) {
            }
        }
        src += strlen(src) + 1;
    }
    
    if (isCut) {
        clipPath[0] = 0;
        clipOp = 0;
        isCut = 0;
    }
    Refresh();
}

void DeleteFiles() {
    char files[MAX_PATH * 100];
    int count;
    GetSelectedFiles(files, &count);
    if (count == 0) return;
    
    if (MessageBox(NULL, "Delete selected items?", "Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    
    char* p = files;
    while (*p) {
        DWORD attr = GetFileAttributes(p);
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirectory(p);
        } else {
            DeleteFile(p);
        }
        p += strlen(p) + 1;
    }
    Refresh();
}

void DoRename() {
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
    
    char newName[MAX_PATH];
    strcpy(newName, name);
    
    HWND hEdit = CreateWindow("EDIT", newName, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 280, 20, hList, NULL, NULL, NULL);
    SetFocus(hEdit);
    
    RECT rc;
    ListView_GetItemRect(hList, sel, &rc, LVIR_LABEL);
    SetWindowPos(hEdit, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
    
    MSG msg;
    int done = 0;
    while (!done && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) {
                GetWindowText(hEdit, newName, MAX_PATH);
                done = 1;
            } else if (msg.wParam == VK_ESCAPE) {
                done = 2;
            }
        } else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN) {
            done = 2;
        }
        if (done) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DestroyWindow(hEdit);
    
    if (done == 1 && newName[0] && strcmp(name, newName)) {
        char oldPath[MAX_PATH], newPath[MAX_PATH];
        sprintf(oldPath, "%s\\%s", currentPath, name);
        sprintf(newPath, "%s\\%s", currentPath, newName);
        MoveFile(oldPath, newPath);
        Refresh();
    }
}

void ShowContextMenu(HWND hwnd, int x, int y) {
    char files[MAX_PATH * 100];
    int count;
    GetSelectedFiles(files, &count);
    
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenu(hMenu, MF_STRING, OPC_NEWFOLDER, "New Folder");
    AppendMenu(hMenu, MF_STRING, OPC_NEWFILE, "New File");
    AppendMenu(hMenu, MF_SEPARATOR, 0, "");
    
    if (count > 0) {
        AppendMenu(hMenu, MF_STRING, OPC_OPEN, "Open");
        AppendMenu(hMenu, MF_SEPARATOR, 0, "");
        AppendMenu(hMenu, MF_STRING, OPC_COPY, "Copy");
        AppendMenu(hMenu, MF_STRING, OPC_CUT, "Cut");
        if (clipOp) AppendMenu(hMenu, MF_STRING, OPC_PASTE, "Paste");
        AppendMenu(hMenu, MF_SEPARATOR, 0, "");
        if (count == 1) AppendMenu(hMenu, MF_STRING, OPC_RENAME, "Rename");
        AppendMenu(hMenu, MF_STRING, OPC_DELETE, "Delete");
        AppendMenu(hMenu, MF_SEPARATOR, 0, "");
    } else if (clipOp) {
        AppendMenu(hMenu, MF_STRING, OPC_PASTE, "Paste");
        AppendMenu(hMenu, MF_SEPARATOR, 0, "");
    }
    
    AppendMenu(hMenu, MF_STRING, OPC_PROPERTIES, "Properties");
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);
    
    switch (cmd) {
        case OPC_NEWFOLDER: CreateNewFolder(); break;
        case OPC_NEWFILE: CreateNewFile(); break;
        case OPC_OPEN: OpenSelectedItem(); break;
        case OPC_COPY: CopyFiles(); break;
        case OPC_CUT: CutFiles(); break;
        case OPC_PASTE: PasteFiles(); break;
        case OPC_DELETE: DeleteFiles(); break;
        case OPC_RENAME: DoRename(); break;
        case OPC_PROPERTIES:
            if (count > 0) {
                char files[MAX_PATH * 100];
                int c;
                GetSelectedFiles(files, &c);
                char* firstFile = files;
                SHELLEXECUTEINFO sei = {0};
                sei.cbSize = sizeof(sei);
                sei.fMask = SEE_MASK_INVOKEIDLIST;
                sei.hwnd = hwnd;
                sei.lpFile = firstFile;
                sei.lpVerb = "properties";
                sei.nShow = SW_SHOWNORMAL;
                ShellExecuteEx(&sei);
            }
            break;
    }
    
    DestroyMenu(hMenu);
}

void HandleDrop(HDROP hDrop) {
    char files[MAX_PATH * 100];
    int count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
    
    files[0] = 0;
    for (int i = 0; i < count; i++) {
        char fp[MAX_PATH];
        DragQueryFile(hDrop, i, fp, MAX_PATH);
        strcat(files, fp);
        files[strlen(fp) + 1] = 0;
    }
    files[strlen(files) + 1] = 0;
    DragFinish(hDrop);
    
    char* src = files;
    while (*src) {
        char name[MAX_PATH];
        char* p = strrchr(src, '\\');
        if (p) strcpy(name, p + 1);
        else strcpy(name, src);
        
        char dst[MAX_PATH];
        sprintf(dst, "%s\\%s", currentPath, name);
        
        if (CopyFile(src, dst, FALSE)) {
        }
        src += strlen(src) + 1;
    }
    Refresh();
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
        case WM_KEYDOWN:
            if (wp == VK_F2) {
                int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                if (sel >= 0) ListView_EditLabel(hList, sel);
            } else if (wp == VK_DELETE) {
                DeleteFiles();
            } else if (wp == VK_F5) {
                Refresh();
            }
            return 0;
        case WM_DROPFILES: HandleDrop((HDROP)wp); return 0;
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
            } else if (pnm->idFrom == ID_LIST_VIEW && pnm->code == LVN_ENDLABELEDIT) {
                NMLVDISPINFO* plvdi = (NMLVDISPINFO*)lp;
                if (plvdi->item.pszText && plvdi->item.iItem >= 0) {
                    char oldName[MAX_PATH];
                    LVITEM lvi = {0};
                    lvi.mask = LVIF_TEXT;
                    lvi.iItem = plvdi->item.iItem;
                    lvi.pszText = oldName;
                    lvi.cchTextMax = MAX_PATH;
                    ListView_GetItem(hList, &lvi);
                    
                    if (strcmp(oldName, plvdi->item.pszText) != 0) {
                        char oldPath[MAX_PATH], newPath[MAX_PATH];
                        sprintf(oldPath, "%s\\%s", currentPath, oldName);
                        sprintf(newPath, "%s\\%s", currentPath, plvdi->item.pszText);
                        MoveFile(oldPath, newPath);
                        Refresh();
                    }
                }
            } else if (pnm->idFrom == ID_LIST_VIEW && pnm->code == NM_RCLICK) {
                DWORD pos = GetMessagePos();
                int x = LOWORD(pos);
                int y = HIWORD(pos);
                
                POINT pt = {x, y};
                ScreenToClient(hList, &pt);
                LVHITTESTINFO lvhti;
                lvhti.pt = pt;
                ListView_HitTest(hList, &lvhti);
                if (lvhti.iItem >= 0) {
                    if (!(ListView_GetItemState(hList, lvhti.iItem, LVIS_SELECTED) & LVIS_SELECTED)) {
                        ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
                        ListView_SetItemState(hList, lvhti.iItem, LVIS_SELECTED, LVIS_SELECTED);
                    }
                }
                
                ShowContextMenu(hwnd, x, y);
            }
            return 0;
        }
        case WM_TIMER:
            if (wp == 2 && editItem >= 0) {
                KillTimer(hList, 2);
                HWND hEdit = ListView_EditLabel(hList, editItem);
                if (hEdit) {
                    SendMessage(hEdit, EM_SETSEL, 0, -1);
                    SetFocus(hEdit);
                }
                editItem = -1;
            }
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
