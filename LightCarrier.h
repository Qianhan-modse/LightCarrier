#pragma once
#ifdef LIGHTCARRIER_EXPORTS
#define LIGHTCARRIER_API __declspec(dllexport)
#else
#define LIGHTCARRIER_API __declspec(dllimport)
#endif

#include <Windows.h>

//标准回调函数指针定义（--stadcall for C# interop）
typedef void(__stdcall* TickCallback)(double);
//初始化并启动线程
LIGHTCARRIER_API int  StartLightCarrierThread(TickCallback callback, double maiTick);

//停止并清理线程
LIGHTCARRIER_API void StopLightCarrierThread(void);

//动态更新Tick值
LIGHTCARRIER_API void UpdateMainTick(double newMainTick);