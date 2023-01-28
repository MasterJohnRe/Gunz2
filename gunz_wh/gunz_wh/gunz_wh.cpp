// memory_editing_first_project.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include "gunz_wh.h"
#include <iterator>
#include <iomanip>
#include <fstream>
#include <vector>
#include <tlhelp32.h>
#include "gunz_wh.h"
#include <sstream>

const int NUM_OF_PLAYERS = 2;
const LPCSTR gunzHandleName = (LPCSTR)"Gunz the Second Duel 1.0.0.55354";
const LPCSTR lf2HandleName = (LPCSTR)"Little Fighter 2";
const std::vector<unsigned int> LF2_OFFSETS = { 0x368, 0x77C, 0x9F8, 0x10, 0x24 , 0x4,0xB4C };
const std::vector<unsigned int> GUNZ2_PLAYER2_OFFSETS = { 0x1C, 0x0, 0x154, 0x17C, 0x0 , 0x74,0x98 };
const std::vector<unsigned int> GUNZ2_PLAYER_OFFSETS = { 0x20, 0x0, 0x154, 0x178, 0x0 , 0x74,0x98 };
const std::vector<unsigned int> GUNZ2_VIEW_MATRIX_OFFSETS = { 0x920 };
const wchar_t* gunzModName = L"Gunz2_Steam.exe";
const wchar_t* lf2ModName = L"lf2.exe";
const DWORD MATRIX_POSITION = 0x11FDED7C;
unsigned int GUNZ_POSITION_BASE_OFFSET = 0x015f81f4;
unsigned int GUNZ_VIEW_MATRIX_BASE_OFFSET = 0x006606E0;
unsigned int lf2HpBaseOffset = 0x000591CC;


//ViewAngles
RECT m_Rect;


//Set of initail variabls you'll need
//Our desktop handle
HDC HDC_Desktop;

//Brush to paint ESP etc
HBRUSH EnemyBrush;
HFONT Font;

//ESP VARS
HWND TargetWnd;
HWND Handle;
DWORD DwProcId;

COLORREF SnapLineColor;
COLORREF TextColor;

typedef struct {
    float flMatrix[4][4];
}WorldToScreenMatrix_t;


//We will use this struct for player
struct MyPlayer_t {
    
    WorldToScreenMatrix_t WorldToScreenMatrix;
    float Position[3];
    void readInformation(HANDLE process , DWORD playerPositionAddr, DWORD matrixAddr) {

        //Reading out our position to our "Position" Variable
        int success = ReadProcessMemory(process, (PBYTE*)playerPositionAddr, Position,sizeof(float[3]),0);
        //VMATRIX
        success = ReadProcessMemory(process, (PBYTE*)MATRIX_POSITION, &WorldToScreenMatrix, sizeof(WorldToScreenMatrix), 0);
    }
}MyPlayer;

struct PlayerList_t
{
    float Position[3];
    
    void readInformation(HANDLE process, DWORD playerPositionAddr) {

        //Reading out our position to our "Position" Variable
        int success = ReadProcessMemory(process, (PBYTE*)playerPositionAddr, Position, sizeof(float[3]), 0);

    }
}PlayerList[2];



void DrawLine(float Startx, float StartY, float EndX, float EndY, COLORREF Pen) {
    int a, b = 0;
    HPEN hOPen;
    //penstyle , width , color
    HPEN hNPen = CreatePen(PS_SOLID, 2, Pen);
    hOPen = (HPEN)SelectObject(HDC_Desktop, hNPen);
    //starting point of line
    MoveToEx(HDC_Desktop, EndX, EndY, NULL);
    //ending point of line
    a = LineTo(HDC_Desktop, EndX, EndY);
    DeleteObject(SelectObject(HDC_Desktop, hOPen));
}

void DrawString(int x, int y, COLORREF color, const char* text) {
    SetTextAlign(HDC_Desktop, TA_CENTER | TA_NOUPDATECP);
    SetBkColor(HDC_Desktop, RGB(0, 0, 0));
    SetBkMode(HDC_Desktop, TRANSPARENT);
    SetTextColor(HDC_Desktop, color);
    SelectObject(HDC_Desktop, Font);
    TextOutA(HDC_Desktop, x, y, text, strlen(text));
    DeleteObject(Font);
}

void DrawFilledRect(int x, int y, int w, int h) {
    //We create our rectangle to draw on screen
    RECT rect = { x ,y , x + w , y + h };
    //We clear that portion of the screen and display our rectangle
    FillRect(HDC_Desktop, &rect, EnemyBrush);
}

void DrawBorderBox(int x, int y, int w, int h, int thickness) {
    //Top horiz line
    DrawFilledRect(x, y, w, thickness);
    //Left vertical line
    DrawFilledRect(x, y, thickness, h);
    //right vertical line
    DrawFilledRect((x + w), y, thickness, h);
    //bottom horiz line
    DrawFilledRect(x, y + h, w + thickness, thickness);

}

void SetupDrawing(HDC hDesktop, HWND handle) {
    HDC_Desktop = hDesktop;
    Handle = handle;
    EnemyBrush = CreateSolidBrush(RGB(255, 0, 0));
    //Color
    SnapLineColor = RGB(255, 0, 0);
    TextColor = RGB(255, 0, 0);
}

float Get3dDistance(float* myCoords, float* enemyCoords) {
    return sqrt(
        pow(double(enemyCoords[0] - myCoords[0]), 2) +
        pow(double(enemyCoords[1] - myCoords[1]), 2) +
        pow(double(enemyCoords[2] - myCoords[2]), 2));

}



bool WorldToScreen(float* from, float* to) {
    float w = 0.0f;
    to[0] = MyPlayer.WorldToScreenMatrix.flMatrix[0][0] * from[0] + MyPlayer.WorldToScreenMatrix.flMatrix[0][1] * from[1] + MyPlayer.WorldToScreenMatrix.flMatrix[0][2] * from[2] + MyPlayer.WorldToScreenMatrix.flMatrix[0][3];
    to[1] = MyPlayer.WorldToScreenMatrix.flMatrix[1][0] * from[0] + MyPlayer.WorldToScreenMatrix.flMatrix[1][1] * from[1] + MyPlayer.WorldToScreenMatrix.flMatrix[1][2] * from[2] + MyPlayer.WorldToScreenMatrix.flMatrix[1][3];
    w = MyPlayer.WorldToScreenMatrix.flMatrix[3][0] * from[0] + MyPlayer.WorldToScreenMatrix.flMatrix[3][1] * from[1] + MyPlayer.WorldToScreenMatrix.flMatrix[3][2] * from[2] + MyPlayer.WorldToScreenMatrix.flMatrix[3][3];

    if (w < 0.00001f)
        return false;

    float invw = 1.0f / w;
    to[0] *= invw;
    to[1] *= invw;
    int width = (int)(m_Rect.right - m_Rect.left);
    int height = (int)(m_Rect.bottom - m_Rect.top);

    float x = width / 2;
    float y = height / 2;
    x += 0.5 * to[0] * width + 0.5;
    y -= 0.5 * to[1] * height + 0.5;

    to[0] = x + m_Rect.left;
    to[1] = y + m_Rect.top;

    return true;

}

void drawESP(int x, int y, float distance) {
    int width = 100;//18100/distance;
    int height = 150;//36000/distance
    DrawBorderBox(x - (width / 2), y - height, width, height, 1);

    //Sandwich ++
    DrawLine((m_Rect.right - m_Rect.left) / 2, m_Rect.bottom - m_Rect.top, x, y, SnapLineColor);
    std::stringstream ss{};
    ss << (int)distance;
    char* distanceInfo = new char[ss.str().size() + 1];
    strcpy(distanceInfo, ss.str().c_str());
    DrawString(x, y, TextColor, distanceInfo);
    delete[] distanceInfo;
}

void ESP(HANDLE process, DWORD playerPositionAddr) {
    GetWindowRect(FindWindow(NULL, (LPCWSTR)gunzHandleName), &m_Rect);

    for (int i = 0; i < NUM_OF_PLAYERS; i++) {
        PlayerList[i].readInformation(process, playerPositionAddr);

        float EnemyXY[3];
        if (WorldToScreen(PlayerList[i].Position, EnemyXY))

            drawESP(EnemyXY[0] - m_Rect.left, EnemyXY[1] - m_Rect.top, Get3dDistance(MyPlayer.Position, PlayerList[i].Position));
    }
}

int main()
{   //get Gunz Window Process Id and store it in PID
    DWORD PID = getGamePID();
    HANDLE process = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
        FALSE,
        PID
    );
    DWORD dynamicPtrPlayerBaseAddr = GetModuleBaseAddress(PID, gunzModName);
    DWORD dynamicPtrViewMatrixBaseAddress = dynamicPtrPlayerBaseAddr;

    DWORD player2Addr;
    DWORD dw_vMatrix;
    DWORD playerAddr;

    dynamicPtrPlayerBaseAddr += GUNZ_POSITION_BASE_OFFSET;
    dynamicPtrViewMatrixBaseAddress += GUNZ_VIEW_MATRIX_BASE_OFFSET;
    std::cout << std::hex;
    DWORD issuccedded;
    issuccedded = ReadProcessMemory(process, (BYTE*)dynamicPtrPlayerBaseAddr, &playerAddr, sizeof(DWORD), 0);
    issuccedded = ReadProcessMemory(process, (BYTE*)dynamicPtrPlayerBaseAddr, &player2Addr, sizeof(DWORD), 0);
    issuccedded = ReadProcessMemory(process, (BYTE*)dynamicPtrViewMatrixBaseAddress, &dw_vMatrix, sizeof(DWORD), 0);
    

    //Resolve the pointer chain
    playerAddr = FindDMAAddy(process, playerAddr, GUNZ2_PLAYER_OFFSETS);
    player2Addr = FindDMAAddy(process, player2Addr, GUNZ2_PLAYER2_OFFSETS);
    dw_vMatrix = FindDMAAddy(process, dw_vMatrix, GUNZ2_VIEW_MATRIX_OFFSETS);
    //print result
    std::cout << std::hex;
    //std::cout << player2Addr << std::endl;
    //std::cout << dw_vMatrix;
    TargetWnd = ::FindWindowEx(0, 0, (LPCWSTR)"Gunz the Second Duel 1.0.0.55354",0);
    HDC HDC_Desktop = GetDC(TargetWnd);
    SetupDrawing(HDC_Desktop, TargetWnd);

    //Our infinite loop will go here
    for (;;) {
        MyPlayer.readInformation(process , playerAddr, dw_vMatrix);
        std::cout << playerAddr << std::endl;
        ESP(process , player2Addr);
    }
    

    return 0;

}





uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName)
{
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32First(hSnap, &modEntry))
        {
            do
            {
                if (!_wcsicmp(modEntry.szModule, modName))
                {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32Next(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}

DWORD getGamePID() {
    HWND myWindow = FindWindowA(NULL, gunzHandleName);
    DWORD PID;
    GetWindowThreadProcessId(myWindow, &PID);
    return PID;
}



uintptr_t FindDMAAddy(HANDLE hProc, uintptr_t ptr, std::vector<unsigned int> offsets)
{
    DWORD player2Addr = ptr;
    int readProcessResult;
    for (unsigned int i = 0; i < offsets.size() - 1; ++i)
    {
        player2Addr += offsets[i];
        readProcessResult = ReadProcessMemory(hProc, (BYTE *)player2Addr, &player2Addr, sizeof(DWORD), 0);
        
    }
    return player2Addr + offsets[offsets.size() - 1];
}



