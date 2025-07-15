#pragma once
#ifdef LIGHTCARRIER_EXPORTS
#define LIGHTCARRIER_API __declspec(dllexport)
#else
#define LIGHTCARRIER_API __declspec(dllimport)
#endif

#include <Windows.h>

//��׼�ص�����ָ�붨�壨--stadcall for C# interop��
typedef void(__stdcall* TickCallback)(double);
//��ʼ���������߳�
LIGHTCARRIER_API int  StartLightCarrierThread(TickCallback callback, double maiTick);

//ֹͣ�������߳�
LIGHTCARRIER_API void StopLightCarrierThread(void);

//��̬����Tickֵ
LIGHTCARRIER_API void UpdateMainTick(double newMainTick);