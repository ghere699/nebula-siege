#pragma once

// add your own read/write here

inline uintptr_t g_imagebase;

inline DWORD GetProcessIdByName(const std::wstring& processName) {
	PROCESSENTRY32W processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32W);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	if (Process32FirstW(snapshot, &processEntry)) {
		do {
			if (processName == processEntry.szExeFile) {
				CloseHandle(snapshot);
				return processEntry.th32ProcessID;
			}
		} while (Process32NextW(snapshot, &processEntry));
	}

	CloseHandle(snapshot);
	return 0;
}