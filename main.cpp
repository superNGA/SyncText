//=========================================================================
//                      SyncText                                         
//=========================================================================
// by      : INSANE                                                        
// created : 25/11/2025
//                                                                         
// purpose : Align symbols across multiple lines.                                                   
//-------------------------------------------------------------------------

/*
The implementation sits entierly in this single source file. 
We use win32 for hanlding window n drawing n stuff.

INDEX : 

Includes.
Structs & Enums
KeyBinds         <- Might wanna change these?
Colors & Padding <- Might wanna change these?
Global Varibles.
Function declerations.
Main Fn
WinProc.
Function definition ( same order as decleration. ).

*/

#include <cstddef>
#define WIN32_LEAN_AND_MEAN

// Window's BS
#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include <cstdint>
#include <WinUser.h>
#include <winuser.h>

// STL stuff...
#include <vector>
#include <unordered_set>
#include <chrono>
#include <string>
#include <sstream>




///////////////////////////////////////////////////////////////////////////
enum CaretMoveMode_t : int32_t
{
    CaretMoveMode_None       = -1,
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

    inline void Clear()
    {
        m_vecTokens.clear(); m_iCaretTokenIndex = -1; m_bComment = false;
    }

    std::vector<Token_t> m_vecTokens;
    int32_t              m_iCaretTokenIndex = -1;
    bool                 m_bComment         = false;

} TextLine_t;
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
typedef struct UserCmd_t
{
    void Clear()
    {
        m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_None;
        m_iMoveAmount    = 1;
        m_symbol         = '\0';
        m_bForward       = true;
    }

    bool IsValid()
    {
        if(m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_FindSymbol && m_symbol != '\0')
            return true;

        if(m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_CountSymbol && m_iMoveAmount > 0)
            return true;

        return false;
    }


    CaretMoveMode_t m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_None;
    int32_t         m_iMoveAmount    = 1;
    char            m_symbol         = '\0';
    bool            m_bForward       = true; // Move direction.

} UserCmd_t;
///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////
typedef struct Vec2
{
    Vec2(int _x, int _y) { x = _x; y = _y; }

    int x, y;
} Vec2;
///////////////////////////////////////////////////////////////////////////




///////////////////////////////////////////////////////////////////////////
enum KeyBinds_t : char
{
    KeyBind_FindSymbolForward   = 'f',
    KeyBind_FindSymbolBackward  = 'F',
    KeyBind_CountTokensForward  = 'w',
    KeyBind_CountTokensBackward = 'b',
    KeyBind_StickyComma         = 's',
    KeyBind_AlignComments       = 'c',
    KeyBind_DrawHelpInfo        = 'h'
};
///////////////////////////////////////////////////////////////////////////




// Test
///////////////////////////////////////////////////////////////////////////
const COLORREF g_bgClr                 = RGB( 25,  25, 25);
const COLORREF g_fgClr                 = RGB( 55,  55, 55);
const COLORREF g_textClr               = RGB(255, 255, 255);
const COLORREF g_caretClr              = RGB(176,   7, 15);

const int      g_iPaddingInPxl         = 10.0f;
const int      g_iPaddingTextBoxInPxl  = 10.0f;
const int      g_iDebugInfoGap         = 10.0f;
///////////////////////////////////////////////////////////////////////////




// Global constants... 
const char*              g_szWindowClass                = "SyncTextWindowClass";
const char*              g_szWindowName                 = "SyncText";
const char*              g_szFontName                   = "JetBrains Mono";
const int                g_iHotKeyModifier              = MOD_CONTROL; //MOD_ALT;
const int                g_iHotKey                      = 'M'; // change according to preferences.
std::string              g_szCmdBuffer                  = ""; // Cmd buffer.
UserCmd_t                g_cmd;

// Extra Features...
bool g_bStickyComma   = true;
bool g_bAlignComments = false;
bool g_bDrawHelpInfo  = true;

// Delimiters that are also tokens...
std::unordered_set<char> m_setDelimiterTokens           = {'"', '\'', ',', ';', '+', '-', '=', '{', '}', '[', ']', '(', ')', '.'};
std::string              g_szComment                    = "//";
std::string              g_szBuffer                     = "";
std::vector<TextLine_t>  g_vecBuffer                    = {}; // Processed buffer.

bool g_bResizeWindow = false;

// Max times we check if we have successfully copied selected text into the clipboard.
constexpr int     SYNCTEXT_MAX_CLIPBOARD_UDPATE_CHECKS           = 10;
constexpr int64_t SYNCTEXT_CLIPBOARD_UPDATE_CHECK_INTERVAL_IN_MS = 100LL; // Interval between each check ( in milliseconds ).




///////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool        SyncText_InitializeWindow        (HINSTANCE hInstance);
int         SyncText_PaintWindow             (HWND hwnd);

// Handling ClipBoard...
void        SyncText_StoreClipBoardText      ();
std::string SyncText_GetClipBoardText        ();
bool        SyncText_SetClipBoardText        (const std::string& szText);
void        SyncText_DumpBufferToClipBoard   (std::vector<TextLine_t>& vecBuffer);

// alignment logic...
bool        SyncText_ProcessBufferV2         (std::string& szBuffer, std::vector<TextLine_t>& vecBufferOut);
char        SyncText_VkToChar                (int vk);
bool        SyncText_IsCmdValid              ();
void        SyncText_ClearCmd                ();
void        SyncText_CaptureKey              (WPARAM wParam, std::string& szCmdBuffer);
bool        SyncText_ProcessCmdBuffer        (std::string& szCmdBuffer, UserCmd_t& cmdOut, std::string& szNewCmd);
void        SyncText_UpdateCaretPos          (std::vector<TextLine_t>& vecBuffer, std::string& szCmdBuffer, UserCmd_t& cmd);
void        SyncText_UpdateCaretPosCountToken(std::vector<TextLine_t>& vecBuffer, const UserCmd_t& cmd);
void        SyncText_UpdateCaretPosFindSymbol(std::vector<TextLine_t>& vecBuffer, const UserCmd_t& cmd);
void        SyncText_ApplyCmd                (std::vector<TextLine_t>& vecBuffer);
std::string SyncText_ContructStringFromBuffer(std::vector<TextLine_t>& vecBuffer);
bool        SyncText_IsLineComment           (TextLine_t& line, const std::string& szComment);

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
                if(g_cmd.IsValid() == true)
                {
                    SyncText_ApplyCmd(g_vecBuffer);
                    g_cmd.Clear();

                    InvalidateRect(hwnd, NULL, true);
                    ShowWindow(hwnd, SW_SHOW);
                    g_bResizeWindow = true; // since text moved, resize might be required.
                }
                else 
                {
                    SyncText_DumpBufferToClipBoard(g_vecBuffer);
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            else if(wParam == VK_ESCAPE)
            {
                if(g_szCmdBuffer.empty() == false)
                {
                    g_szCmdBuffer.clear();
                }
                else
                {
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            else 
            {
                SyncText_CaptureKey(wParam, g_szCmdBuffer);
                SyncText_UpdateCaretPos(g_vecBuffer, g_szCmdBuffer, g_cmd);
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
            Vec2 vScreenSize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            Vec2 vWindowSize(static_cast<int>(wParam), static_cast<int>(lParam));

            // Clamp window size to some minimum.
            if(vWindowSize.x < 50) vWindowSize.x = 50;
            if(vWindowSize.y < 50) vWindowSize.y = 50;

            MoveWindow(hwnd, (vScreenSize.x - vWindowSize.x) / 2, (vScreenSize.y - vWindowSize.y) / 2, vWindowSize.x, vWindowSize.y, true);
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
    //wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.hbrBackground = NULL;
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


    ShowWindow(hWindow, SW_HIDE);
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

    // Set font.
    auto oldFont = SelectObject(hdc, s_pFont);

    // Font dimensions...
    TEXTMETRIC tm; GetTextMetrics(hdc, &tm);
    int iCharWidth  = tm.tmAveCharWidth;
    int iLineHeight = tm.tmHeight + tm.tmExternalLeading;


    // BG Color
    HBRUSH hBGClrBrush = CreateSolidBrush(g_bgClr); // dark blue, pick any color
    FillRect(hdc, &ps.rcPaint, hBGClrBrush);
    DeleteObject(hBGClrBrush);


    // so text draws properly.
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_textClr);


    Vec2 vCursorPos(g_iPaddingInPxl, g_iPaddingInPxl);


    // Calculating textbox width & height.
    Vec2 vTextBoxSize(g_iPaddingTextBoxInPxl * 2, g_iPaddingTextBoxInPxl * 2);

    int iMaxLineWidth = 0;
    for(TextLine_t& line : g_vecBuffer) // finding thickest line.
    {
        if(line.m_vecTokens.empty() == true)
            continue;

        int iLineWidth = line.m_vecTokens.back().m_iAbsIndex + line.m_vecTokens.back().m_szToken.size();
        
        if(iLineWidth > iMaxLineWidth)
            iMaxLineWidth = iLineWidth; 
    }
    vTextBoxSize.x += iMaxLineWidth      * iCharWidth;
    vTextBoxSize.y += g_vecBuffer.size() * iLineHeight;



    // Padding constant...
    {
        auto hBrush    = CreateSolidBrush(g_fgClr);
        auto hNoPen    = GetStockObject  (NULL_PEN);
        auto hOldBrush = SelectObject    (hdc, hBrush);
        auto hOldPen   = SelectObject    (hdc, hNoPen);

        Rectangle(hdc, g_iPaddingInPxl, g_iPaddingInPxl, vTextBoxSize.x + g_iPaddingInPxl, vTextBoxSize.y + g_iPaddingInPxl);

        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hBrush);
    }


    // Drawing text... ( All tokens. )
    {
        Vec2 vCursorPos(g_iPaddingInPxl + g_iPaddingTextBoxInPxl, g_iPaddingInPxl + g_iPaddingTextBoxInPxl);

        for(const TextLine_t& line : g_vecBuffer)
        {
            for(const Token_t& token : line.m_vecTokens)
            {
                TextOutA(hdc, vCursorPos.x + (token.m_iAbsIndex * iCharWidth), vCursorPos.y, token.m_szToken.c_str(), token.m_szToken.size());
            }


            // Next line.
            vCursorPos.y += iLineHeight;
        }
    }


    // Drawing carets... ( one per line. )
    {
        Vec2 vCursorPos(g_iPaddingInPxl + g_iPaddingTextBoxInPxl, g_iPaddingInPxl + g_iPaddingTextBoxInPxl);

        auto hPen      = CreatePen(PS_SOLID, 1, g_caretClr);
        auto hBrush    = GetStockObject(NULL_BRUSH);
        auto hOldPen   = SelectObject(hdc, hPen);
        auto hOldBrush = SelectObject(hdc, hBrush);
        for(size_t iLineIndex = 0; iLineIndex < g_vecBuffer.size(); iLineIndex++)
        {
            TextLine_t& line = g_vecBuffer[iLineIndex];

            if(line.m_iCaretTokenIndex < 0)
                 continue;

            if(line.m_vecTokens.empty() == true)
                continue;

            int iCaretTokenIndex = SyncText_Clamp<int>(line.m_iCaretTokenIndex, 0, line.m_vecTokens.size() - 1LLU);

            int x = line.m_vecTokens[iCaretTokenIndex].m_iAbsIndex * iCharWidth;
            int y = iLineIndex                                     * iLineHeight;
            
            Rectangle(hdc, vCursorPos.x + x, vCursorPos.y + y, vCursorPos.x + x + iCharWidth, vCursorPos.y + y + iLineHeight);
        }
        SelectObject(hdc, hOldBrush);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);
    }



    // Text box done, so add its size.
    vCursorPos.y += vTextBoxSize.y;
    

    // Drawing other information.
    if(g_bDrawHelpInfo == true)
    {
        vCursorPos.y += g_iDebugInfoGap;

        std::stringstream debugInfoStream;
        debugInfoStream << "CMD : " << g_szCmdBuffer;
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        vCursorPos.y += iLineHeight;

        debugInfoStream << KeyBinds_t::KeyBind_FindSymbolForward << " : Find symbol in front";
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        vCursorPos.y += iLineHeight;

        debugInfoStream << KeyBinds_t::KeyBind_FindSymbolBackward << " : Find symbol behind";
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        vCursorPos.y += iLineHeight;

        debugInfoStream << KeyBinds_t::KeyBind_CountTokensForward << " : Move to n-th symbol in front";
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        vCursorPos.y += iLineHeight;

        debugInfoStream << KeyBinds_t::KeyBind_CountTokensBackward << " : Move to n-th symbol behind";
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        vCursorPos.y += iLineHeight;

        SetTextColor(hdc, g_bStickyComma == true ? RGB(0, 255, 0) : RGB(255, 0, 0));

        debugInfoStream << "Sticky Comma   : " << (g_bStickyComma == true ? "[ Enabled ]" : "[ Disabled ]");
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        SetTextColor(hdc, g_textClr);

        vCursorPos.y += iLineHeight;

        SetTextColor(hdc, g_bAlignComments == true ? RGB(0, 255, 0) : RGB(255, 0, 0));

        debugInfoStream << "Align Comments : " << (g_bAlignComments == true ? "[ Enabled ]" : "[ Disabled ]");
        TextOutA(hdc, vCursorPos.x, vCursorPos.y, debugInfoStream.str().c_str(), debugInfoStream.str().size());
        debugInfoStream.str(""); debugInfoStream.clear();

        SetTextColor(hdc, g_textClr);

        vCursorPos.y += iLineHeight;
    }

    vCursorPos.y += g_iPaddingInPxl;


    // Resize window if required...
    if(g_bResizeWindow == true)
    {
        PostMessage(hwnd, WM_USER + 2, vTextBoxSize.x + (2 * g_iPaddingInPxl), vCursorPos.y);
        g_bResizeWindow = false;
    }


    SelectObject(hdc, oldFont);
    
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
    keybd_event(VK_CONTROL, 0,               0, 0);
    keybd_event(       'C', 0,               0, 0);
    keybd_event(       'C', 0, KEYEVENTF_KEYUP, 0);
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
    SyncText_ProcessBufferV2(g_szBuffer, g_vecBuffer);


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
void SyncText_DumpBufferToClipBoard(std::vector<TextLine_t>& vecBuffer)
{
    std::string szOutput = SyncText_ContructStringFromBuffer(vecBuffer);
    SyncText_SetClipBoardText(szOutput);
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_ProcessBufferV2(std::string& szBuffer, std::vector<TextLine_t>& vecBufferOut)
{
    vecBufferOut.clear();

    if(szBuffer.empty() == true)
        return false;


    // Fuck you microsoft!
    szBuffer.erase(std::remove(szBuffer.begin(), szBuffer.end(), '\r'), szBuffer.end());
    

    // Temp line object. Will be reused.
    TextLine_t line; line.Clear();

    
    size_t iLineStartIndex = 0LLU;
    size_t iBufferSize     = szBuffer.size();
    for(size_t iIndex = 0; iIndex <= iBufferSize; iIndex++)
    {
        char c = szBuffer[iIndex];

        if(c == ' ')
            continue;

        
        for(size_t iTokenIndex = iIndex; iTokenIndex < iBufferSize; iTokenIndex++)
        {
            char curChar  = szBuffer[iTokenIndex];
            char nextChar = szBuffer[iTokenIndex + 1LLU];


            // Cur char is delimiter.
            if(m_setDelimiterTokens.count(curChar) > 0LLU)
            {
                // NOTE : delimiters are expected to be single character.
                Token_t token; 
                token.m_szToken   = curChar; //szBuffer.substr(iTokenIndex, iTokenIndex - iIndex + 1LLU);
                token.m_iAbsIndex = iIndex - iLineStartIndex;
                line.m_vecTokens.push_back(token);

                iIndex            = iTokenIndex;

                break;
            }
            // Line ended?
            else if(curChar == '\n' || curChar == '\0')
            {
                iLineStartIndex = iTokenIndex + 1LLU;
                line.m_bComment = SyncText_IsLineComment(line, g_szComment);

                vecBufferOut.push_back(line);
                line.Clear();

                break;
            }
            // Next char is a delimiters.
            else if(nextChar == ' ' || m_setDelimiterTokens.count(nextChar) > 0LLU || nextChar == '\n' || nextChar == '\0')
            {
                Token_t token;
                token.m_szToken   = szBuffer.substr(iIndex, iTokenIndex - iIndex + 1LLU);
                token.m_iAbsIndex = iIndex - iLineStartIndex;
                line.m_vecTokens.push_back(token);

                iIndex            = iTokenIndex;

                break;
            }
        }


        if(c == '\0')
            break;
    }

    return true;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
char SyncText_VkToChar(int vk)
{
    char c         = '\0';

    bool bCapsLock = GetKeyState(VK_CAPITAL) & 1;
    bool bShift    = GetKeyState(VK_SHIFT)   & 0x8000;


    if (vk >= 'A' && vk <= 'Z') 
    {
        c = static_cast<char>(vk); // Upper case be default.

        if(bShift == false && bCapsLock == false)
        {
            c = (c - 'A') + 'a';
        }
    }

    
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
    if(bShift == true)
    {
        switch (c) 
        {
        case '0':  c = ')'; break;
        case '1':  c = '!'; break;
        case '2':  c = '@'; break;
        case '3':  c = '#'; break;
        case '4':  c = '$'; break;
        case '5':  c = '%'; break;
        case '6':  c = '^'; break;
        case '7':  c = '&'; break;
        case '8':  c = '*'; break;
        case '9':  c = '('; break;
        case ']':  c = '}'; break;
        case '[':  c = '{'; break;
        case '\'': c = '"'; break;
        case ';' : c = ':'; break;
        case '.':  c = '>'; break;
        case ',':  c = '<'; break;
        case '/':  c = '?'; break;
        
        default: break;
        }
    }


    return c;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_IsCmdValid()
{
    return g_szCmdBuffer.empty() == false;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_ClearCmd()
{
    g_szCmdBuffer.clear();
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_CaptureKey(WPARAM wParam, std::string& szCmdBuffer)
{
    char c = SyncText_VkToChar(wParam);
    
    if(c == '\0')
        return;

    
    switch(c)
    {
    case KeyBind_AlignComments:
        g_bAlignComments = !g_bAlignComments;
        break;

    case KeyBind_StickyComma:
        g_bStickyComma   = !g_bStickyComma;
        break;

    case KeyBind_DrawHelpInfo:
        g_bDrawHelpInfo  = !g_bDrawHelpInfo;
        g_bResizeWindow  = true;
        break;

    default: break;
    }


    szCmdBuffer.push_back(c);
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_ProcessCmdBuffer(std::string& szCmdBuffer, UserCmd_t& cmdOut, std::string& szNewCmd)
{
    if(szCmdBuffer.empty() == true)
        return false;


    cmdOut.Clear();


    // index of character that set the caret move mode in the UserCmd_t;
    int iMoveModeKeyIndex = 0;
    

    // Find the first occuring move mode setting character.
    size_t nCharacters = szCmdBuffer.size();
    for(size_t iIndex = 0LLU; iIndex < nCharacters; iIndex++)
    {
        char c = szCmdBuffer[iIndex];

        switch (c) 
        {
        case KeyBind_FindSymbolForward: 
            cmdOut.m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_FindSymbol; 
            cmdOut.m_bForward       = true; 
            iMoveModeKeyIndex       = static_cast<int>(iIndex);
            break;

        case KeyBind_FindSymbolBackward:
            cmdOut.m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_FindSymbol;
            cmdOut.m_bForward       = false;
            iMoveModeKeyIndex       = static_cast<int>(iIndex);
            break;

        case KeyBind_CountTokensForward:
            cmdOut.m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_CountSymbol;
            cmdOut.m_bForward       = true;
            iMoveModeKeyIndex       = static_cast<int>(iIndex);
            break;

        case KeyBind_CountTokensBackward:
            cmdOut.m_iCaretMoveMode = CaretMoveMode_t::CaretMoveMode_CountSymbol;
            cmdOut.m_bForward       = false;
            iMoveModeKeyIndex       = static_cast<int>(iIndex);
            break;

        default: break;
        }


        // If move mode is found then no need to bother with other move modes.
        if(cmdOut.m_iCaretMoveMode != CaretMoveMode_t::CaretMoveMode_None)
            break;
    }


    if(cmdOut.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_None)
        return false;



    // Find Symbol or Move amount.
    int iCmdEndIndex = iMoveModeKeyIndex; // final character of the buffer we need to clip to.
    if(cmdOut.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_FindSymbol)
    {
        int iSymbolIndex = iMoveModeKeyIndex + 1;
        
        if(iSymbolIndex < szCmdBuffer.size())
        {
            cmdOut.m_symbol = szCmdBuffer[iSymbolIndex];
            iCmdEndIndex    = iSymbolIndex;
        }
    }
    else if(cmdOut.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_CountSymbol)
    {
        int iMultiplier = 1;
        int iMoveAmount = 1;
        for(int i = iMoveModeKeyIndex - 1; i >= 0; i--)
        {
            if(szCmdBuffer[i] > '9' || szCmdBuffer[i] < '0')
                break;

            int iNum = szCmdBuffer[i] - '0';
            iMoveAmount += iNum * iMultiplier;
            iMultiplier *= 10;
        }

        cmdOut.m_iMoveAmount = iMoveAmount;
    }


    // Did we catch any complete cmd? ( move mode + symbol or move amount )
    if(cmdOut.m_symbol == '\0' && cmdOut.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_FindSymbol)
        return false;

    
    szNewCmd.clear();
    if(iCmdEndIndex <= szCmdBuffer.size() - 1LLU)
    {
        szNewCmd = szCmdBuffer.substr(iCmdEndIndex + 1, szCmdBuffer.size() - iCmdEndIndex - 1);
    }


    return true;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UpdateCaretPos(std::vector<TextLine_t>& vecBuffer, std::string& szCmdBuffer, UserCmd_t& cmd)
{
    if(szCmdBuffer.empty() == true)
        return;

    
    // Parse and construct user cmd.
    std::string szNewCmdBuffer("");
    if(SyncText_ProcessCmdBuffer(szCmdBuffer, cmd, szNewCmdBuffer) == false)
        return;

    szCmdBuffer = szNewCmdBuffer;

    
    if(cmd.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_CountSymbol)
    {
        SyncText_UpdateCaretPosCountToken(vecBuffer, cmd);
    }
    else if(cmd.m_iCaretMoveMode == CaretMoveMode_t::CaretMoveMode_FindSymbol)
    {
        SyncText_UpdateCaretPosFindSymbol(vecBuffer, cmd);
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UpdateCaretPosCountToken(std::vector<TextLine_t>& vecBuffer, const UserCmd_t& cmd)
{
    for(TextLine_t& line : vecBuffer)
    {
        if(line.m_vecTokens.empty() == true)
            continue;

        if(line.m_bComment == true && g_bAlignComments == false)
            continue;

        line.m_iCaretTokenIndex += cmd.m_bForward == true ? cmd.m_iMoveAmount : cmd.m_iMoveAmount * -1;
        line.m_iCaretTokenIndex  = SyncText_Clamp<int>(line.m_iCaretTokenIndex, 0, line.m_vecTokens.size() - 1LLU);
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_UpdateCaretPosFindSymbol(std::vector<TextLine_t>& vecBuffer, const UserCmd_t& cmd)
{
    // Bad shit, no symbol found.
    assert(cmd.m_symbol != '\0' && "Symbol is null!!!");
    if(cmd.m_symbol == '\0')
        return;



    // NOTE : Don't use size_t here, m_iCaretTokenIndex can be negative and casting would cause underflow.
    for(TextLine_t& line : vecBuffer)
    {
        if(line.m_bComment == true && g_bAlignComments == false)
            continue;


        if(cmd.m_bForward == true)
        {
            for(int iTokenIndex = line.m_iCaretTokenIndex + 1LLU; iTokenIndex < static_cast<int>(line.m_vecTokens.size()); iTokenIndex++)
            {
                if(line.m_vecTokens[iTokenIndex].m_szToken.empty() == true)
                    continue;

                if(line.m_vecTokens[iTokenIndex].m_szToken[0] == cmd.m_symbol)
                {
                    line.m_iCaretTokenIndex = iTokenIndex;
                    break;
                }
            }
        }
        else 
        {
            for(int iTokenIndex = line.m_iCaretTokenIndex - 1; iTokenIndex >= 0; iTokenIndex--)
            {
                if(line.m_vecTokens[iTokenIndex].m_szToken.empty() == true)
                    continue;

                if(line.m_vecTokens[iTokenIndex].m_szToken[0] == cmd.m_symbol)
                {
                    line.m_iCaretTokenIndex = iTokenIndex;
                    break;
                }
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void SyncText_ApplyCmd(std::vector<TextLine_t>& vecBuffer)
{
    int iTargetCaretIndex = -1;


    // Finding target caret pos.
    for(TextLine_t& line : vecBuffer)
    {
        if(line.m_vecTokens.empty() == true)
            continue;

        if(line.m_iCaretTokenIndex < 0)
            continue;

        int iCaretIndex = SyncText_Clamp<int>(line.m_iCaretTokenIndex, 0, line.m_vecTokens.size() - 1LLU);
        if(line.m_vecTokens[iCaretIndex].m_iAbsIndex > iTargetCaretIndex)
        {
            iTargetCaretIndex = line.m_vecTokens[iCaretIndex].m_iAbsIndex;
        }
    }


    // No target caret pos found?
    if(iTargetCaretIndex < 0)
        return;

    
    // Modify all lines so that the caret is aligned in all lines.
    for(TextLine_t& line : vecBuffer)
    {
        // Negative caret index means this line doesn't have the desired
        // symbol that we want to align. Leave it alone.
        if(line.m_iCaretTokenIndex < 0)
            continue;

        if(line.m_bComment == true && g_bAlignComments == false)
            continue;


        int iOffset = -1;
        for(size_t iTokenIndex = 0LLU; iTokenIndex < line.m_vecTokens.size(); iTokenIndex++)
        {
            Token_t& token = line.m_vecTokens[iTokenIndex];

            // If this is the token where the caret is curret at, then calc. and store offset
            // that we need to apply to all the remaining tokens in this line.
            if(line.m_iCaretTokenIndex == iTokenIndex && iOffset < 0)
            {
                iOffset = iTargetCaretIndex - token.m_iAbsIndex;

                // If "sticky comma" is enabled then pull along the some tokens before it.
                // 3 or more in case when the token before this comma is a string in double or single quotes.
                if(token.m_szToken[0] == ',' && g_bStickyComma == true)
                {
                    if(iTokenIndex > 0)
                    {
                        int iStickyTokenIndex = iTokenIndex - 1;

                        // We will pull the first token regardless.
                        line.m_vecTokens[iStickyTokenIndex].m_iAbsIndex += iOffset;

                        char c = line.m_vecTokens[iStickyTokenIndex].m_szToken[0];

                        if(c == '\'' || c == '"')
                        {
                            while(iStickyTokenIndex >= 0)
                            {
                                // we did the first preceding token already, go to second preceding token.
                                iStickyTokenIndex--;
                                
                                line.m_vecTokens[iStickyTokenIndex].m_iAbsIndex += iOffset;

                                char s = line.m_vecTokens[iStickyTokenIndex].m_szToken[0];
                                if(s == c)
                                    break;
                            }
                        }
                    }
                }
            }

            if(iOffset <= 0)
                continue;

            token.m_iAbsIndex += iOffset;
        }
    }
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
std::string SyncText_ContructStringFromBuffer(std::vector<TextLine_t>& vecBuffer)
{
    std::stringstream resultStream;
    size_t nLines = vecBuffer.size();

    for(size_t iLineIndex = 0LLU; iLineIndex < nLines; iLineIndex++)
    {
        size_t      iCharIndex = 0LLU;
        TextLine_t& line       = vecBuffer[iLineIndex];

        for(Token_t& token : line.m_vecTokens)
        {
            while(iCharIndex < token.m_iAbsIndex)
            {
                resultStream << ' ';
                iCharIndex++;
            }

            resultStream << token.m_szToken;
            iCharIndex += token.m_szToken.size();
        }


        // if not last line, add new line character.
        if(iLineIndex < nLines - 1LLU)
            resultStream << '\n';
    }


    return resultStream.str();
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
bool SyncText_IsLineComment(TextLine_t& line, const std::string& szComment)
{
    if(line.m_vecTokens.empty() == true)
        return false;


    int      iCharIterator  = 0;
    int      iTokenIterator = 0;
    Token_t* pActiveToken   = &line.m_vecTokens[iTokenIterator];

    for(const char c : szComment)
    {
        if(iCharIterator >= pActiveToken->m_szToken.size())
        {
            // Does it has one more token?
            if(iTokenIterator + 1 >= line.m_vecTokens.size())
                return false;

            iTokenIterator += 1;
            pActiveToken    = &line.m_vecTokens[iTokenIterator];
        }

        if(c != pActiveToken->m_szToken[iCharIterator])
            return false;

        iCharIterator++;
    }


    return true;
}