
#pragma region Includes
// ��� WINAPI � ������ �������
#include <Windows.h>
#include <intrin.h>
#include <vector>
#include <tchar.h>
#include <fstream> 
#include <iostream>
#pragma intrinsic(_ReturnAddress)
// �������� �������
#include <MinHook.h>
#include <string>
using namespace std;
#pragma comment(lib,"libMinHook.x86.lib")
#pragma endregion


BOOL __stdcall IsEnemy( int UnitAddr );