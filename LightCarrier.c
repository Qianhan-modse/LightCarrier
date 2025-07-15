#include "LightCarrier.h"
#include <stdbool.h>

//高精度等待API声明
typedef LONG(NTAPI* NtDelayExecutionProc)(BOOL, PLARGE_INTEGER);
static NtDelayExecutionProc NtDelayExecution = NULL;

//线程控制块
static HANDLE hWorkerThread = NULL;
static volatile bool isRunning = false;
static double currentMainTick = 0.0F;

//高精度计时相关
static LARGE_INTEGER frequency;
static LARGE_INTEGER nextTickTime;

//线程同步锁
static CRITICAL_SECTION syncLock;

//线程工作函数
DWORD WINAPI WorkerThread(LPVOID lpParma)
{
	TickCallback callback = (TickCallback)lpParma; // 获取回调函数指针
	LARGE_INTEGER currentTime;
	//初始化高精度计时器
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&nextTickTime);

	//循环
	while (isRunning)
	{
		//计算下一个tick的时间点
		double interval = currentMainTick;
		LONGLONG nextTickDelta = (LONGLONG)(interval * frequency.QuadPart);
		nextTickTime.QuadPart += nextTickDelta;

		//精确等待到下一个tick时间点
		while (1)
		{
			QueryPerformanceCounter(&currentTime);
			LONGLONG timeLeft = nextTickTime.QuadPart - currentTime.QuadPart;
			if (timeLeft <= 0)break;//已超时
			//可精确睡眠
			if (timeLeft > frequency.QuadPart / 1000)
			{
				Sleep(1); // 睡眠1毫秒
			}
			else
			{
				//微秒级精确等待
				int microSec = (int)((double)timeLeft * 1000000 / frequency.QuadPart);
				if (microSec > 0)
				{
					//使用高精度等待API
					LARGE_INTEGER waitTime;
					waitTime.QuadPart = -1 * microSec * 10; // 100ns单位
					NtDelayExecution(FALSE, &waitTime);
				}
			}
		}
		//保护回调执行
		__try {
			//获取当前最新的tick值
			EnterCriticalSection(&syncLock);
			double currentTick = currentMainTick;
			LeaveCriticalSection(&syncLock);
			callback(currentTick); // 调用回调函数
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			//处理异常 -记录错误但不崩溃
			OutputDebugStringA("Exception in TickCallback handler");
			//可扩展为记录日志或其他错误处理
		}
	}
	return 0; //线程结束
}
//启动线程
LIGHTCARRIER_API int StartLightCarrierThread(TickCallback callback, double mainTick)
{
	if (isRunning || callback == NULL)
	{
		return 0; // 已经在运行或回调函数无效
	}

	//初始化临界区
	InitializeCriticalSection(&syncLock);

	EnterCriticalSection(&syncLock);
	
	currentMainTick = mainTick;
	
	isRunning = true;
	
	hWorkerThread = CreateThread(
	
		NULL,//默认安全属性 
		
		0, //默认堆栈大小
		
		WorkerThread, //线程函数指针
		
		(LPVOID)callback, // 传递回调函数指针
		
		0, //默认创建标志
		
		NULL//不需要线程标识符/不需要线程ID
	);
	return hWorkerThread != NULL ? 1 : 0; // 返回线程句柄是否有效
}
//停止线程
LIGHTCARRIER_API void StopLightCarrierThread(void)
{
	if (!isRunning)return; // 如果线程未运行则直接返回

	EnterCriticalSection(&syncLock); // 进入临界区保护

	isRunning = false; // 设置运行标志为false
	
	LeaveCriticalSection(&syncLock); // 离开临界区保护

	if (hWorkerThread)
	{
		//安全终止线程
		if (WaitForSingleObject(hWorkerThread, 1000) == WAIT_TIMEOUT)
		{
			OutputDebugStringA("[LightCarrier] 既然在一定时间内你不肯走，那么我只好“优雅”的踢掉你.");
			//优雅退出失败，强制终止
			TerminateThread(hWorkerThread, 0);
		}
		CloseHandle(hWorkerThread); // 关闭线程句柄
		hWorkerThread = NULL; // 清空线程句柄
	}
	DeleteCriticalSection(&syncLock); // 删除临界区
}

//更新MainTick值
LIGHTCARRIER_API void UpdateMainTick(double newMainTick)
{
	EnterCriticalSection(&syncLock); // 进入临界区保护
	currentMainTick = newMainTick; // 更新当前Tick值
	LeaveCriticalSection(&syncLock); // 离开临界区保护
}

//DLL入口点 -初始化高精度等待API
BOOL WINAPI DLLMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
		if (hNtDll)
		{
			NtDelayExecution = (NtDelayExecutionProc)GetProcAddress(hNtDll, "NtDelayExecution");
		}
	}
	return TRUE; // 成功加载DLL
}