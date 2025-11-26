//=========================================================================
//                      SyncText                                         
//=========================================================================
// by      : INSANE                                                        
// created : 25/11/2025
//                                                                         
// purpose : Align those fucking '=' signs!!!!!!!!!!!                                                  
//-------------------------------------------------------------------------
#include <cmath>
#include <cstdlib>
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
    CaretMoveMode_FindSymbol = 0,
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
    int32_t              m_iCaretIndex = 0;
    bool                 m_bComment    = false;

} TextLine_t;
///////////////////////////////////////////////////////////////////////////




// Global constants... 
const char*              g_szWindowClass                = "SyncTextWindowClass";
const char*              g_szWindowName                 = "SyncText";
const char*              g_szFontName                   = "JetBrains Mono";
const int                g_iHotKeyModifier              = MOD_CONTROL; //MOD_ALT;
const int                g_iHotKey                      = 'M'; // change according to preferences.
const int                g_iKeyCaretMoveModeFindSymbol  = 'F';
const int                g_iKeyCaretMoveModeCountSymbol = 'W';
CaretMoveMode_t          g_iCaretMoveMode               = CaretMoveMode_t::CaretMoveMode_None;
std::string              g_szCmd                        = ""; // This key defines how the caretMoveMode functions. ( stored in ASCII )

// Delimiters that are also tokens...
std::unordered_set<char> m_setDelimiterTokens           = {'"', '\'', ',', ';', '+', '-', '=', '{', '}', '[', ']', '(', ')'};
std::string              g_szComment                    = "//";
std::string              g_szBuffer                     = "";
std::vector<TextLine_t>  g_vecBuffer                    = {}; // Processed buffer.
std::vector<std::string> vecLines; // Delete this, just for debugging.

// Window dimensions...
bool g_bResizeWindow = false;

// Max times we check if we have successfully copied selected text into the clipboard.
constexpr int     SYNCTEXT_MAX_CLIPBOARD_UDPATE_CHECKS           = 10;
constexpr int64_t SYNCTEXT_CLIPBOARD_UPDATE_CHECK_INTERVAL_IN_MS = 100LL; // Interval between each check ( in milliseconds ).




///////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool        SyncText_InitializeWindow(HINSTANCE hInstance);
int         SyncText_PaintWindow(HWND hwnd);

// Handling ClipBoard...
void        SyncText_StoreClipBoardText();
std::string SyncText_GetClipBoardText();
bool        SyncText_SetClipBoardText(const std::string& szText);

// alignment logic...
bool        SyncText_ProcessBuffer(std::string& szBuffer, std::vector<TextLine_t>& vecBufferOut);
char        SyncText_VkToChar(int vk);
void        SyncText_ClearCmd();
void        SyncText_HandleKey(WPARAM wParam);
void        SyncText_UpdateCaretPos(std::vector<TextLine_t>& vecBuffer, CaretMoveMode_t iCaretMode, const std::string& szCmd);
void        SyncText_UpdateCaretPosCountToken(std::vector<TextLine_t>& vecBuffer, int iTokenIndex);
void        SyncText_UdpateCaretPosFindSymbol(std::vector<TextLine_t>& vecBuffer, char symbol);

// Util...
template<typename T>
T SyncText_Clamp(T data, T min, T max) { return data < min ? min : (data > max ? max : data); }
///////////////////////////////////////////////////////////////////////////




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
            {
                ShowWindow(hwnd, SW_HIDE);
            }
            else if(wParam == VK_ESCAPE)
            {
                if(g_iCaretMoveMode != CaretMoveMode_t::CaretMoveMode_None)
                {
                    SyncText_ClearCmd();
                }
                else 
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            else 
            {
                SyncText_HandleKey(wParam);
                SyncText_UpdateCaretPos(g_vecBuffer, g_iCaretMoveMode, g_szCmd);
            }
            InvalidateRect(hwnd, NULL, TRUE);

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

            // Restore Caret move mode & cmd.
            SyncText_ClearCmd();

            // Recalculate window size & resize accordingly.
            g_bResizeWindow = true;

            // Enable the window...
            InvalidateRect(hwnd, NULL, TRUE);
            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
            return 0;


        // TODO : Add a option list. Ask to quit in the option
        case WM_USER + 1:
            if (lParam == WM_RBUTTONUP) 
                PostQuitMessage(0);

            return 0;

        case WM_USER + 2:
        {
            int iWindowWidth  = static_cast<int>(wParam);
            int iWindowHeight = static_cast<int>(lParam);
            int iScreenWidth  = GetSystemMetrics(SM_CXSCREEN);
            int iScreenHeight = GetSystemMetrics(SM_CYSCREEN);
            MoveWindow(hwnd, (iScreenWidth - iWindowWidth) / 2, (iScreenHeight - iWindowHeight) / 2, iWindowWidth, iWindowHeight, true);
            return 0;
        }

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


    static HFONT s_pFont = nullptr;
    if(s_pFont == nullptr)
    {
        s_pFont = CreateFont(
            -16, 0, 0, 0, FW_NORMAL,                 
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,   
            g_szFontName
        );
    }

    auto oldObject = SelectObject(hdc, s_pFont);

    // For dimensions of our font.
    TEXTMETRIC tm; GetTextMetrics(hdc, &tm);

    //  Since I am expecing a mono font, this should be good.
    int iCharWidth  = tm.tmAveCharWidth;
    int iLineHeight = tm.tmHeight + tm.tmExternalLeading;


    // Drawing tokens...
    int y = 0;
    int iXMax = 0, iXMin = INFINITE;
    for(TextLine_t& line : g_vecBuffer)
    {
        for(Token_t& token : line.m_vecTokens)
        {
            TextOutA(hdc, token.m_iAbsIndex * iCharWidth, y, token.m_szToken.c_str(), token.m_szToken.size());


            // In case we need to resize our window, calculate the min & max.
            if(g_bResizeWindow == true)
            {
                size_t iTokenMax = static_cast<size_t>(token.m_iAbsIndex) + token.m_szToken.size();
                size_t iTokenMin = token.m_iAbsIndex;
                
                if(iTokenMin < iXMin)
                {
                    iXMin = iTokenMin;
                }
                else if(iTokenMax > iXMax)
                {
                    iXMax = iTokenMax;
                }
            }
        }

        y += iLineHeight;
    }


    // Drawing caret pos...
    HPEN   hPen     = CreatePen(PS_SOLID, 1, RGB(0,0,0));
    HBRUSH hNoBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    auto hOldPen   = SelectObject(hdc, hPen);
    auto hOldBrush = SelectObject(hdc, hNoBrush);
    for(size_t iLineIndex = 0; iLineIndex < g_vecBuffer.size(); iLineIndex++)
    {
        TextLine_t& line = g_vecBuffer[iLineIndex];

        if(line.m_iCaretIndex < 0)
            continue;

        int x = line.m_iCaretIndex * iCharWidth;
        int y = iLineIndex * iLineHeight;
        Rectangle(hdc, x, y, x + iCharWidth, y + iLineHeight);
    }
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);


    // Debugging information...
    y += iLineHeight;
    {
        std::stringstream debugInfoStream;
        debugInfoStream << "Mode : ";
        switch (g_iCaretMoveMode) 
        {
        case CaretMoveMode_FindSymbol:
            debugInfoStream << "Find Symbol";
            break;

        case CaretMoveMode_CountSymbol:
            debugInfoStream << "Count Token";
            break;

        default:
            debugInfoStream << "Invalid";
            break;
        }

        debugInfoStream << ",  Modifier char : " << g_szCmd;
        TextOutA(hdc, 0, y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        

        y += iLineHeight;


        debugInfoStream.str(""); debugInfoStream.clear();
        debugInfoStream << "CharWidth : " << iCharWidth << ", LineHeight : "<< iLineHeight;
        TextOutA(hdc, 0, y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
    }
    y += iLineHeight;


    // Resize window if required...
    if(g_bResizeWindow == true)
    {
        int iWindowHeight = y;
        int iWindowWidth  = iXMax * iCharWidth;
        PostMessage(hwnd, WM_USER + 2, iWindowWidth, iWindowHeight);

        g_bResizeWindow = false;
    }


    // Restore old font.
    SelectObject(hdc, oldObject);
    
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
    vecBufferOut.clear();
    for(std::string& szLine : vecLines)
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

        vecBufferOut.push_back(line);
    }


    return true;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
char SyncText_VkToChar(int vk)
{
    char c = '\0';

    if (vk >= 'A' && vk <= 'Z') 
        c = static_cast<char>(vk);  // base lowercase
    
    if (vk >= '0' && vk <= '9')
        c = static_cast<char>(vk);

    switch (vk)
    {
        case VK_SPACE:     c = ' ';  break;
        case VK_OEM_MINUS: c = '-';  break;
        case VK_OEM_PLUS:  c = '=';  break;
        case VK_OEM_COMMA: c = ',';  break;
        case VK_OEM_PERIOD:c = '.';  break;
        case VK_OEM_1:     c = ';';  break;
        case VK_OEM_2:     c = '/';  break;
        case VK_OEM_3:     c = '`';  break;
        case VK_OEM_4:     c = '[';  break;
        case VK_OEM_5:     c = '\\'; break;
        case VK_OEM_6:     c = ']';  break;
        case VK_OEM_7:     c = '\''; break;
    }


    // Handling +shift key cases.
    if(GetKeyState(VK_SHIFT) & 0x8000)
    {
        switch (c) 
        {
        case '0':  c = ')'; break;
        case '9':  c = '('; break;
        case ']':  c = '}'; break;
        case '[':  c = '{'; break;
        case '\'': c = '"'; break;
        
        default: break;
        }
    }

    return c;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_ClearCmd()
{
    g_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_None;
    g_szCmd.clear();
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_HandleKey(WPARAM wParam)
{
    if(g_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_None)
    {
        if(wParam == g_iKeyCaretMoveModeFindSymbol)
        {
            g_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_FindSymbol;
        }
        else if(wParam == g_iKeyCaretMoveModeCountSymbol)
        {
            g_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_CountSymbol;
        }
    }
    else 
    {
        char c = SyncText_VkToChar(wParam);
        
        // If caret move mode is findSymbol, the only one cmd key is allowed.
        if(g_iCaretMoveMode == CaretMoveMode_FindSymbol)
        {
            g_szCmd = c;
        }
        else 
        {
            if(c >= '0' && c <= '9')
            {
                g_szCmd.push_back(c);
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UpdateCaretPos(std::vector<TextLine_t>& vecBuffer, CaretMoveMode_t iCaretMode, const std::string& szCmd)
{
    if(iCaretMode == CaretMoveMode_t::CaretMoveMode_None)
        return;

    
    // We should have a non zero cmd.
    if(szCmd.empty() == true)
        return;

    
    if(iCaretMode == CaretMoveMode_t::CaretMoveMode_CountSymbol)
    {
        int iTokenIndex = atoi(szCmd.c_str());
        SyncText_UpdateCaretPosCountToken(vecBuffer, iTokenIndex);
    }
    else if(iCaretMode == CaretMoveMode_t::CaretMoveMode_FindSymbol)
    {
        SyncText_UdpateCaretPosFindSymbol(vecBuffer, szCmd[0]);
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UpdateCaretPosCountToken(std::vector<TextLine_t>& vecBuffer, int iTokenIndex)
{
    for(TextLine_t& line : vecBuffer)
    {
        int iTargetTokenIndex = line.m_iCaretIndex + iTokenIndex;
        iTargetTokenIndex = SyncText_Clamp<int>(iTargetTokenIndex, 0, line.m_vecTokens.size() - 1LLU);

        line.m_iCaretIndex = line.m_vecTokens[iTargetTokenIndex].m_iAbsIndex;
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UdpateCaretPosFindSymbol(std::vector<TextLine_t>& vecBuffer, char symbol)
{
    for(TextLine_t& line : vecBuffer)
    {
        // Iterating all tokens & finding the first occuring token ( from current caret pos )
        // with first character same as desired symbol.
        for(const Token_t& token : line.m_vecTokens)
        {
            if(token.m_iAbsIndex < line.m_iCaretIndex)
                continue;

            if(token.m_szToken.empty() == true)
                continue;

            if(token.m_szToken[0] == symbol)
                line.m_iCaretIndex = token.m_iAbsIndex;
        }
    }
}