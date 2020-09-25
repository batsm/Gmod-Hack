#include <windows.h>
#include <iostream>
#include <math.h>
#include "HackProcess.h"
#include <string>
#include <vector>
#include <algorithm>

// Sets's Game Process Variable
CHackProcess  fProcess;

using namespace std;

// Variables
const DWORD PlayerAddress = 0x0062F8CC;
const DWORD EntityAddress = 0x0064BB6C;
const DWORD dw_health = 0x90;
const DWORD dw_attack = 0x6B2D78;
const DWORD dw_teamOffset = 0x9C;
const DWORD dw_PlayerCountOffset = 0x62B694; // Client
const DWORD dw_CrosshairOffset = 0x2C48;
const DWORD dw_EntityLoopDistance = 0x10;
const DWORD dw_Jump = 0x6B2D84;
const DWORD dw_JumpOffset = 0x350;
const DWORD dw_pos = 0x338;
const DWORD dw_AngRotation = 0x4B74C4; // Engine


bool b_ShotNow = false;
bool b_True = true;
bool b_False = false;

int i_Shoot = 5;
int i_DontShoot = 4;
int NumOfPlayers = 64;
int Count; // Used for NUmOfPlayers alive loop

// Hack Variables
bool b_BunnyHopEnabled = false;
bool b_TriggerBotEnabled = false;
bool b_AimbotEnabled = false;

#define RIGHT_MOUSE 0x02

// ESP  GDI

//Set of initial variables you'll need
//Our desktop handle
HDC HDC_Desktop;
//Brush to paint ESP etc
HBRUSH EnemyBrush;
HFONT Font; //font we use to write text with
COLORREF SnapLineCOLOR;
COLORREF TextCOLOR;
HWND TargetWnd;
HWND Handle;
DWORD DwProcId;

//Receive the 2-D Coordinates the colour and the device we want to use to draw those colours with
//HDC so we know where to draw and brush because we need it to draw
void DrawFilledRect(int x, int y, int w, int h)
{
	//Create our rectangle to draw on screen
	RECT rect = { x, y, x + w, y + h };
	//Clear that portion of the screen and display our rectangle
	FillRect(HDC_Desktop, &rect, EnemyBrush);
}


void DrawBorderBox(int x, int y, int w, int h, int thickness)
{
	//Top horiz line
	DrawFilledRect(x, y, w, thickness);
	//Left vertical line
	DrawFilledRect(x, y, thickness, h);
	//right vertical line
	DrawFilledRect((x + w), y, thickness, h);
	//bottom horiz line
	DrawFilledRect(x, y + h, w + thickness, thickness);
}


void SetupDrawing(HDC hDesktop, HWND handle)
{
	HDC_Desktop = hDesktop;
	Handle = handle;
	EnemyBrush = CreateSolidBrush(RGB(255, 0, 0));
	//Color
	SnapLineCOLOR = RGB(0, 0, 255);
	TextCOLOR = RGB(0, 255, 0);
}

//Draw our line from point A to Point B
void DrawLine(float StartX, float StartY, float EndX, float EndY, COLORREF Pen)
{
	int a, b = 0;
	HPEN hOPen;
	// penstyle, width, color
	HPEN hNPen = CreatePen(PS_SOLID, 2, Pen);
	hOPen = (HPEN)SelectObject(HDC_Desktop, hNPen);
	// starting point of line
	MoveToEx(HDC_Desktop, StartX, StartY, NULL);
	// ending point of line
	a = LineTo(HDC_Desktop, EndX, EndY);
	DeleteObject(SelectObject(HDC_Desktop, hOPen));
}

//Draw our text with this function
void DrawString(int x, int y, COLORREF color, const char* text)
{
	SetTextAlign(HDC_Desktop, TA_CENTER | TA_NOUPDATECP);

	SetBkColor(HDC_Desktop, RGB(0, 0, 0));
	SetBkMode(HDC_Desktop, TRANSPARENT);

	SetTextColor(HDC_Desktop, color);

	SelectObject(HDC_Desktop, Font);

	TextOutA(HDC_Desktop, x, y, text, strlen(text));

	DeleteObject(Font);
}


//
// Draw code finished
// Functionality code below
//

// Read Player Information
struct MyPlayer_t
{
	DWORD CLocalPlayer;
	int Team;
	int CrosshairEntityID;
	int Health;
	int m_fFlags;
	float Position[3];

	void ReadInfo()
	{
		// Local Player Base
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + PlayerAddress), &CLocalPlayer, sizeof(DWORD), 0);
		// Team
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_teamOffset), &Team, sizeof(int), 0);
		// Crosshair
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_CrosshairOffset), &CrosshairEntityID, sizeof(int), 0);
		// Jump
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_JumpOffset), &m_fFlags, sizeof(int), 0);
		// Health
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_health), &Health, sizeof(int), 0);
		// Position
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CLocalPlayer + dw_pos), &Position, sizeof(float[3]), 0);
	}
}MyPlayer;


// Get's all players coords
struct TargetList_t
{
	float Distance;
	float Angle[3];

	TargetList_t()
	{
		// Default Constructor
	}

	TargetList_t(float angle[], float myCoords[], float EnemyCoords[])
	{
		Distance = Get3dDistance(myCoords[0], myCoords[1], myCoords[2],
								EnemyCoords[0], EnemyCoords[1], EnemyCoords[2]);
		Angle[0] = angle[0];
		Angle[1] = angle[1];
		Angle[2] = angle[2];
	}

	// Get Player Distance
	float Get3dDistance(float myCoordsX, float myCoordsZ, float myCoordsY,
						float EnX, float EnZ, float EnY)
	{
		return sqrt(pow(double(EnX - myCoordsX), 2.0) + 
					pow(double(EnY - myCoordsY), 2.0) +
					pow(double(EnZ - myCoordsZ), 2.0));
	}
};


// Sorting
struct CompareTargetEnArray
{
	bool operator () (TargetList_t & lhs, TargetList_t & rhs)
	{
		return lhs.Distance < rhs.Distance;
	}
};


// World to screen calculation
void CalcAngle(float *src, float *dst, float *angles)
{
	// Screen Src - Dist
	double delta[3] = { (src[0] - dst[0]), (src[1] - dst[1]), (src[2] - dst[2])};
	// Get's the hypotenuse
	double hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
	// SinF
	angles[0] = (float)(asinf(delta[2] / hyp) * 57.295779513082f);
	// TanF
	//angles[1] = (float)(atanf(delta[1] / delta[0]) * 57.295779513082f);
	angles[1] = (float)(atanf(delta[1] / delta[0]) * 57.295779513082f);
	// Reset
	angles[2] = 0.0f;

	// FOV
	if (delta[0] >= 0.0)
	{
		angles[1] += 180.0f;
	}
}


// Player Entity Base - Read Info
struct PlayerList_t
{
	DWORD CBaseEntity;
	int Team;
	int Health;
	float Position[3];
	float Angle[3];

	void ReadInfo(int Player)
	{
		// Base
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + EntityAddress +(Player* dw_EntityLoopDistance)), &CBaseEntity, sizeof(DWORD), 0);
		// Team
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_teamOffset), &Team, sizeof(int), 0);
		// Health
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_health), &Health, sizeof(int), 0);
		// Position
		ReadProcessMemory(fProcess.__HandleProcess, (PBYTE*)(CBaseEntity + dw_pos), &Position, sizeof(float[3]), 0);
	}
}PlayerList[64];



void TriggerBot()
{
	if(GetAsyncKeyState(VK_CAPITAL))
	{
	// Disable Attack
	if (!b_ShotNow)
	{
		WriteProcessMemory(fProcess.__HandleProcess, (int*)(fProcess.__dwordClient + dw_attack), &i_DontShoot, sizeof(int), 0);
		b_ShotNow = true;
	}

	if (MyPlayer.CrosshairEntityID == 0)
	{
		return;
	}

	//if (PlayerList[MyPlayer.CrosshairEntityID - 1].Team == MyPlayer.Team)
	//{
	//	return;
	//}

	if (MyPlayer.CrosshairEntityID > NumOfPlayers)
	{
		return;
	}

	// Enable Attack
	if (b_ShotNow)
	{
		WriteProcessMemory(fProcess.__HandleProcess, (int*)(fProcess.__dwordClient + dw_attack), &i_Shoot, sizeof(int), 0);
		b_ShotNow = false;
	}
	}
}



void Aimbot()
{
	Count = 0;

	TargetList_t *TargetList = new TargetList_t[NumOfPlayers];
	int TargetLoop = 0;

	for (int i = 0; i < NumOfPlayers; i++)
	{
		if (PlayerList[i].Team == 1)
		{
			Count++;
		}
	}

	if(Count > 1)
	{
		if (NumOfPlayers > 1)
		{
			if (MyPlayer.Team == 1)
			{
				for (int i = 0; i < NumOfPlayers; i++)
				{
					PlayerList[i].ReadInfo(i);

					if (PlayerList[i].Health < 2)
						continue;


					// Calc Screen Angle XY
					CalcAngle(MyPlayer.Position, PlayerList[i].Position, PlayerList[i].Angle);

					TargetList[TargetLoop] = TargetList_t(PlayerList[i].Angle, MyPlayer.Position, PlayerList[i].Position);

					TargetLoop++;
				}

				// Sort Array
				if (TargetLoop > 0)
				{
					std::sort(TargetList, TargetList + TargetLoop, CompareTargetEnArray());

					// Right Click
					if (GetAsyncKeyState(VK_MBUTTON))
					{
						// Write to Memory Angle
						//WriteProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordEngine + dw_AngRotation), TargetList[0].Angle, 12, 0);	

						WriteProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordEngine + dw_AngRotation), TargetList[1].Angle, 12, 0);
						Sleep(5);
						WriteProcessMemory(fProcess.__HandleProcess, (int*)(fProcess.__dwordClient + dw_attack), &i_Shoot, sizeof(int), 0);
						Sleep(5);
					}else
					WriteProcessMemory(fProcess.__HandleProcess, (int*)(fProcess.__dwordClient + dw_attack), &i_DontShoot, sizeof(int), 0);
					Sleep(5);
				}
				// Reset Loop
				TargetLoop = 0;
				// Prevent Memory Leak
				delete[] TargetList;
			}
		}
	}
}
			


void BunnyHop()
{
	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
	{
		if (MyPlayer.m_fFlags == 257)
		{
			WriteProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + dw_Jump), &b_True, sizeof(bool), 0);
		}
		else if (MyPlayer.m_fFlags = 256)
		{
			WriteProcessMemory(fProcess.__HandleProcess, (PBYTE*)(fProcess.__dwordClient + dw_Jump), &b_False, sizeof(bool), 0);
		}
	}

}


// Hack Loop
int main()
{
	for (;;)
	{
		BunnyHop();
	
			//keybd_event(0x11,0, 0x11,0);
			//keybd_event(0x56, 0, 0x11, 0);
	
	}
}
