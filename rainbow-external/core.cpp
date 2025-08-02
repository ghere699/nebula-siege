#include "includes.hpp"

int main()
{
	SetConsoleTitle("");

	auto pid = GetProcessIdByName(L"RainbowSix.exe");

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

	if (hProcess == NULL) {
		std::cerr << "Failed to open process. Error code: " << GetLastError() << std::endl;
		return 1;
	}

	HMODULE hMods[1024];
	DWORD cbNeeded;
	HMODULE hMainModule = NULL;

	// Enumerate all modules in the process.
	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		// The first module in the returned list is the executable.
		hMainModule = hMods[0];
	}
	else {
		std::cerr << "EnumProcessModules failed. Error code: " << GetLastError() << std::endl;
		CloseHandle(hProcess); // Clean up the handle
		return 1;
	}

	// Clean up the process handle as we're done with it.
	CloseHandle(hProcess);

	if (hMainModule != NULL) {
		g_imagebase = reinterpret_cast<uintptr_t>(hMainModule);
		std::cout << "The base address of the main module is: "
			<< std::showbase
			<< std::hex
			<< g_imagebase;
	}
	else {
		std::cerr << "Could not determine the main module's base address." << std::endl;
		return 1;
	}


	hv::g_cr3 = hv::query_process_cr3(pid);

	overlay.setup_overlay();

	std::thread cache_thread(cache.setup_cache);
	cache_thread.detach();

	std::thread overlay_thread(overlay.setup_render);
	overlay_thread.detach();

#pragma push_macro("max")
#undef max
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
#pragma pop_macro("max")
	std::cin.get();
	return 0;
}