#include "includes.hpp"

int main()
{
	SetConsoleTitle("");

	auto pid = GetProcessIdByName(L"RainbowSix.exe");

	g_imagebase = hv::get_section_base_process(pid);


	hv::g_cr3 = hv::query_process_cr3(pid);

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