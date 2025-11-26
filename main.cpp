//=========================================================================
//                      SyncText                                         
//=========================================================================
// by      : INSANE                                                        
// created : 25/11/2025
//                                                                         
// purpose : Align those fucking '=' signs!!!!!!!!!!!                                                  
//-------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
// Window's BS
#include <windows.h>
#include <cstdint>
#include <windef.h>
#include <handleapi.h>
#include <WinUser.h>
#include <winuser.h>
#include <wingdi.h>
#include <shellapi.h>
#include <sstream>
#include <cstddef>
#include <winbase.h>
#include <synchapi.h>

// STL stuff...
#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>




///////////////////////////////////////////////////////////////////////////
enum CaretMoveMode_t
{
    CaretMoveMode_None = -1,
    CaretMoveMode_FindSymbol,
    CaretMoveMode_CountSymbol
};
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
typedef struct Token_t
{
    // NOTE : m_iAbsIndex for a token will compensate for the spaces and 
    // other stuff in front of it. That is, number of characters in front it
    // this token.

    std::string m_szToken;
    int32_t     m_iAbsIndex = -1;

} Token_t;
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
typedef struct TextLine_t
{
    // Our text buffer will be split at each newline character and each line will be 
    // stored as this "TextLine_t" structure, which shall hold the line in a processed form.

    std::vector<Token_t> m_vecTokens;
    int32_t              m_iCarretIndex = -1;
    bool                 m_bComment     = false;

} TextLine_t;
///////////////////////////////////////////////////////////////////////////




// Global constants... 
const char*  g_szWindowClass   = "SyncTextWindowClass";
const char*  g_szWindowName    = "SyncText";
const int    g_iHotKeyModifier = MOD_CONTROL; //MOD_ALT;
const int    g_iHotKey         = 'M'; // change according to preferences.

// Delimiters that are also tokens...
std::unordered_set<char> m_setDelimiterTokens = {'"', '\'', ',', ';', '+', '-', '=', '{', '}', '[', ']', '(', ')'};
std::string  g_szComment       = "//";
std::string  g_szBuffer        = "";
std::vector<TextLine_t> g_vecBuffer = {}; // Processed buffer.
std::vector<std::string> vecLines; // Delete this, just for debugging.


// Max times we check if we have successfully copied selected text into the clipboard.
constexpr int     SYNCTEXT_MAX_CLIPBOARD_UDPATE_CHECKS           = 10;
constexpr int64_t SYNCTEXT_CLIPBOARD_UPDATE_CHECK_INTERVAL_IN_MS = 100LL; // Interval between each check ( in milliseconds ).




LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool        SyncText_InitializeWindow(HINSTANCE hInstance);
int         SyncText_PaintWindow(HWND hwnd);

// Handling ClipBoard...
void        SyncText_StoreClipBoardText();
std::string SyncText_GetClipBoardText();
bool        SyncText_SetClipBoardText(const std::string& szText);

bool        SyncText_ProcessBuffer(std::string& szBuffer, std::vector<TextLine_t>& vecBufferOut);

// Util...
template<typename T>
T SyncText_Clamp(T data, T min, T max)
{
    return data < min ? min : (data > max ? max : data);
}




///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize tha window. :)
    if(SyncText_InitializeWindow(hInstance) == false)
        return 0;


    // Main loop
    MSG msg;
    while( GetMessage(&msg, 0, 0, 0) == true)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    return 0;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_KEYDOWN: 
            if(wParam == VK_RETURN)
                ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_LBUTTONDOWN: // To drag the window around.
            ReleaseCapture();
            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;

        case WM_PAINT:
            return SyncText_PaintWindow(hwnd);

        case WM_HOTKEY:
            SyncText_StoreClipBoardText();

            // Enable the window...
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
            return 0;

        // TODO : Add a option list. Ask to quit in the option
        case WM_USER + 1:
            if (lParam == WM_RBUTTONUP) 
                PostQuitMessage(0);

            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_InitializeWindow(HINSTANCE hInstance)
{
    WNDCLASS wndClass; memset(&wndClass, 0, sizeof(WNDCLASS));
    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WndProc;
    wndClass.hInstance     = hInstance;
    wndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszClassName = g_szWindowClass;
    wndClass.lpszMenuName  = NULL;
    wndClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    if(RegisterClass(&wndClass) == false)
    {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_ICONERROR);
        return false;
    }


    HWND hWindow = CreateWindowEx(
        0,
        g_szWindowClass, g_szWindowName,
        WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, 
        0, 0, hInstance, 0
    );


    ShowWindow(hWindow, SW_SHOW);
    UpdateWindow(hWindow);


    // Put this window in the hidden icons tray
    NOTIFYICONDATA nid; memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = hWindow;
    nid.uID              = 1;
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    lstrcpy(nid.szTip, g_szWindowName);


    // ADD ICON
    if(Shell_NotifyIcon(NIM_ADD, &nid) == false)
    {
        MessageBoxA(NULL, "Failed \"Shell_NotifyIcon\"", "Error", MB_ICONERROR);
        return false;
    }

    // Give it a hotkey, so we can open it quickly.
    if (RegisterHotKey(hWindow, 1, g_iHotKeyModifier, g_iHotKey) == false)
    {
        MessageBoxA(NULL, "Failed to register hotkey!", "Error", MB_ICONERROR);
        return false;
    }


    return true;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
int SyncText_PaintWindow(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);


    // Delete this, just for debugging.
    std::stringstream ss;
    ss << "Buffer : " << g_szBuffer;
    TextOutA(hdc, 20, 20, ss.str().c_str(), ss.str().size());
    std::stringstream ss2;
    ss2 << "Line count : " << vecLines.size();
    TextOutA(hdc, 20, 40, ss2.str().c_str(), ss2.str().size());

    int y = 100;
    for(TextLine_t line : g_vecBuffer)
    {
        std::stringstream ss;
        ss << "TokenCount : " << line.m_vecTokens.size() << "Comment : " << line.m_bComment;
        TextOutA(hdc, 20, y, ss.str().c_str(), ss.str().size());
        y += 20;

        for(Token_t token : line.m_vecTokens)
        {
            TextOutA(hdc, 20, y, token.m_szToken.c_str(), token.m_szToken.size());
            
            y += 20;
        }

    }


    EndPaint(hwnd, &ps);
    return 0;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_StoreClipBoardText()
{
    // Store old text.
    std::string szOldText = SyncText_GetClipBoardText();


    // Must do this, else nothing will be copied due to some focus issue.
    HWND hPrevWindow = GetForegroundWindow();
    SwitchToThisWindow(hPrevWindow, true);
    Sleep(100);


    // Now we put some simple text in the clipboard, so we know when Ctrl+C 
    // opeartion is completed.
    std::string s_szTriggerText("_FAILED_TO_GET_TEXT_");
    SyncText_SetClipBoardText(s_szTriggerText);


    // Copy...
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);


    // Spin-locking till Ctrl+C operation is completed.
    auto copyCheckStartTime  = std::chrono::high_resolution_clock::now();
    int  nChecks             = SYNCTEXT_MAX_CLIPBOARD_UDPATE_CHECKS;
    while(nChecks > 0)
    {
        int64_t iTimeSinceLastCheck = 
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - copyCheckStartTime).count();
        
        // If 100ms have passed since last check, check again.
        if(iTimeSinceLastCheck > SYNCTEXT_CLIPBOARD_UPDATE_CHECK_INTERVAL_IN_MS)
        {
            copyCheckStartTime = std::chrono::high_resolution_clock::now();
            nChecks--;

            // If clipboard has been modified
            if(SyncText_GetClipBoardText() != s_szTriggerText)
                break;
        }
    }
    

    // Copy and store new clipboard text...
    g_szBuffer = SyncText_GetClipBoardText();
    g_szBuffer.push_back('\0'); // An extra null terminator won't hurt, yes?


    // Chop up the std::string into tokens...
    SyncText_ProcessBuffer(g_szBuffer, g_vecBuffer);


    // Restore clipboard so prevent inconvinence.
    SyncText_SetClipBoardText(szOldText);
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
std::string SyncText_GetClipBoardText()
{
    std::string szResult;

    if (OpenClipboard(NULL) == false)
        return szResult;


    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData != NULL)
    {
        char* pText = reinterpret_cast<char*>(GlobalLock(hData));
        if (pText != nullptr)
        {
            szResult = pText;
            GlobalUnlock(hData);
        }
    }

    CloseClipboard();
    return szResult;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_SetClipBoardText(const std::string& szText)
{
    if(OpenClipboard(NULL) == false)
        return false;


    EmptyClipboard();


    // Copy text to clipboard.
    HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, szText.size() + 1);
    if(hGlob != nullptr)
    {
        void* pData = GlobalLock(hGlob);
        memcpy(pData, szText.c_str(), szText.size() + 1);
        GlobalUnlock(hGlob);

        SetClipboardData(CF_TEXT, hGlob);
    }


    CloseClipboard();
    return true;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_ProcessBuffer(std::string& szBuffer, std::vector<TextLine_t>& vecBufferOut)
{
    //std::vector<std::string> vecLines = {};
    vecLines.clear();

    size_t iLineStartIndex = 0;
    size_t iBufferSize     = szBuffer.size();
    for(size_t iCharIndex = 0; iCharIndex < iBufferSize; iCharIndex++)
    {
        char c = szBuffer[iCharIndex];

        if(c == '\n' || c == '\0')
        {
            size_t iLineSize = SyncText_Clamp<size_t>(iCharIndex - iLineStartIndex + 1LLU, 0LLU, iBufferSize - iLineStartIndex + 1LLU);

            vecLines.push_back(szBuffer.substr(iLineStartIndex, iLineSize));

            iLineStartIndex = SyncText_Clamp<size_t>(iCharIndex + 1LLU, 0LLU, iBufferSize - 1LLU);
        }
    }


    // Now we iterate each line and split it into tokens...
    g_vecBuffer.clear();
    for(std::string szLine : vecLines)
    {
        if(szLine.empty() == true)
            continue;


        TextLine_t line;

        // Check if this line is a comment
        if(szLine.size() >= g_szComment.size())
        {
            bool bComment = true;
            for(int i = 0; i < g_szComment.size(); i++)
            {
                if(szLine[i] != g_szComment[i])
                    bComment = false;   
            }

            line.m_bComment = bComment;
        }


        size_t iTokenStart = 0LLU;
        size_t iLineSize   = szLine.size();
        for(size_t iIndex = 0; iIndex < iLineSize; iIndex++)
        {
            char c = szLine[iIndex];
            
            // a line should not have a newline character, cause we 
            // have already splitted the line at \n characters.
            if(c == '\n' || c == '\0')
            {
                // if token start is pointing at a non-delimiter token character, then we
                // will need to manually include that character into the list cause no 
                // delimiter will occur now.
                if(szLine[iTokenStart] != ' ' && m_setDelimiterTokens.count(szLine[iTokenStart]) == 0LLU)
                {
                    // there should be alteast 1 character between line end & token start index.
                    if(iTokenStart < iIndex)
                    {
                        Token_t token;
                        token.m_szToken   = szLine.substr(iTokenStart, iIndex - iTokenStart);
                        token.m_iAbsIndex = iTokenStart;
                        line.m_vecTokens.push_back(token);
                    }
                }

                break;
            }


            if(m_setDelimiterTokens.count(c) > 0LLU)
            {
                // If the character before this character ( which in this case is a delimiter )
                // is also a delimiter, then we don't need to add it. assuming that it was already
                // added when it appeared as current character 'c'.
                if(iTokenStart < iIndex && m_setDelimiterTokens.count(szLine[iTokenStart]) == 0LLU)
                {
                    Token_t token;
                    token.m_szToken   = szLine.substr(iTokenStart, iIndex - iTokenStart);
                    token.m_iAbsIndex = iTokenStart;
                    line.m_vecTokens.push_back(token);
                }


                // Push in this delimiter token as well.
                Token_t token;
                token.m_szToken   = c;
                token.m_iAbsIndex = iIndex;
                line.m_vecTokens.push_back(token);


                // Now token start represents the next character, which means
                // the current character when we get to next iteration. ( assuming we get in same branch. )
                iTokenStart = iIndex + 1LLU;
            }
            else if(c == ' ')
            {
                if(iTokenStart < iIndex)
                {
                    Token_t token;
                    token.m_szToken   = szLine.substr(iTokenStart, iIndex - iTokenStart);
                    token.m_iAbsIndex = iTokenStart;
                    line.m_vecTokens.push_back(token);
                }

                // Now token start represents the next character, which means
                // the current character when we get to next iteration. ( assuming we get in same branch. )
                iTokenStart = iIndex + 1LLU;
            }
        }        

        g_vecBuffer.push_back(line);
    }


    return true;
}