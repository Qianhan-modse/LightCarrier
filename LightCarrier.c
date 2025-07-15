#include "LightCarrier.h"
#include <stdbool.h>

//�߾��ȵȴ�API����
typedef LONG(NTAPI* NtDelayExecutionProc)(BOOL, PLARGE_INTEGER);
static NtDelayExecutionProc NtDelayExecution = NULL;

//�߳̿��ƿ�
static HANDLE hWorkerThread = NULL;
static volatile bool isRunning = false;
static double currentMainTick = 0.0F;

//�߾��ȼ�ʱ���
static LARGE_INTEGER frequency;
static LARGE_INTEGER nextTickTime;

//�߳�ͬ����
static CRITICAL_SECTION syncLock;

//�̹߳�������
DWORD WINAPI WorkerThread(LPVOID lpParma)
{
	TickCallback callback = (TickCallback)lpParma; // ��ȡ�ص�����ָ��
	LARGE_INTEGER currentTime;
	//��ʼ���߾��ȼ�ʱ��
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&nextTickTime);

	//ѭ��
	while (isRunning)
	{
		//������һ��tick��ʱ���
		double interval = currentMainTick;
		LONGLONG nextTickDelta = (LONGLONG)(interval * frequency.QuadPart);
		nextTickTime.QuadPart += nextTickDelta;

		//��ȷ�ȴ�����һ��tickʱ���
		while (1)
		{
			QueryPerformanceCounter(&currentTime);
			LONGLONG timeLeft = nextTickTime.QuadPart - currentTime.QuadPart;
			if (timeLeft <= 0)break;//�ѳ�ʱ
			//�ɾ�ȷ˯��
			if (timeLeft > frequency.QuadPart / 1000)
			{
				Sleep(1); // ˯��1����
			}
			else
			{
				//΢�뼶��ȷ�ȴ�
				int microSec = (int)((double)timeLeft * 1000000 / frequency.QuadPart);
				if (microSec > 0)
				{
					//ʹ�ø߾��ȵȴ�API
					LARGE_INTEGER waitTime;
					waitTime.QuadPart = -1 * microSec * 10; // 100ns��λ
					NtDelayExecution(FALSE, &waitTime);
				}
			}
		}
		//�����ص�ִ��
		__try {
			//��ȡ��ǰ���µ�tickֵ
			EnterCriticalSection(&syncLock);
			double currentTick = currentMainTick;
			LeaveCriticalSection(&syncLock);
			callback(currentTick); // ���ûص�����
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			//�����쳣 -��¼���󵫲�����
			OutputDebugStringA("Exception in TickCallback handler");
			//����չΪ��¼��־������������
		}
	}
	return 0; //�߳̽���
}
//�����߳�
LIGHTCARRIER_API int StartLightCarrierThread(TickCallback callback, double mainTick)
{
	if (isRunning || callback == NULL)
	{
		return 0; // �Ѿ������л�ص�������Ч
	}

	//��ʼ���ٽ���
	InitializeCriticalSection(&syncLock);

	EnterCriticalSection(&syncLock);
	
	currentMainTick = mainTick;
	
	isRunning = true;
	
	hWorkerThread = CreateThread(
	
		NULL,//Ĭ�ϰ�ȫ���� 
		
		0, //Ĭ�϶�ջ��С
		
		WorkerThread, //�̺߳���ָ��
		
		(LPVOID)callback, // ���ݻص�����ָ��
		
		0, //Ĭ�ϴ�����־
		
		NULL//����Ҫ�̱߳�ʶ��/����Ҫ�߳�ID
	);
	return hWorkerThread != NULL ? 1 : 0; // �����߳̾���Ƿ���Ч
}
//ֹͣ�߳�
LIGHTCARRIER_API void StopLightCarrierThread(void)
{
	if (!isRunning)return; // ����߳�δ������ֱ�ӷ���

	EnterCriticalSection(&syncLock); // �����ٽ�������

	isRunning = false; // �������б�־Ϊfalse
	
	LeaveCriticalSection(&syncLock); // �뿪�ٽ�������

	if (hWorkerThread)
	{
		//��ȫ��ֹ�߳�
		if (WaitForSingleObject(hWorkerThread, 1000) == WAIT_TIMEOUT)
		{
			OutputDebugStringA("[LightCarrier] ��Ȼ��һ��ʱ�����㲻���ߣ���ô��ֻ�á����š����ߵ���.");
			//�����˳�ʧ�ܣ�ǿ����ֹ
			TerminateThread(hWorkerThread, 0);
		}
		CloseHandle(hWorkerThread); // �ر��߳̾��
		hWorkerThread = NULL; // ����߳̾��
	}
	DeleteCriticalSection(&syncLock); // ɾ���ٽ���
}

//����MainTickֵ
LIGHTCARRIER_API void UpdateMainTick(double newMainTick)
{
	EnterCriticalSection(&syncLock); // �����ٽ�������
	currentMainTick = newMainTick; // ���µ�ǰTickֵ
	LeaveCriticalSection(&syncLock); // �뿪�ٽ�������
}

//DLL��ڵ� -��ʼ���߾��ȵȴ�API
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
	return TRUE; // �ɹ�����DLL
}