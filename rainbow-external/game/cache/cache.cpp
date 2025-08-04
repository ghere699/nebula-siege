#include "includes.hpp"
#include <unordered_set>

// this namespace was generated from my dumper which gathers and auto generates shellcode for each build
// I will provide my current build for the latest r6s update (25 June 2025)
// you can go into IDA and find these addresses and make your own sigs for your build...

namespace offset
{
    inline constexpr unsigned long long camera_patch_address = 0xF156BB4;
    inline constexpr unsigned long long actor_patch_address = 0x973679B;
    inline constexpr unsigned long long code_cave_one = 0x11DE3FED;
    inline constexpr unsigned long long code_cave_two = 0x128C10B0;
    inline constexpr unsigned char actor_mov_instruction[3] = { 0xCC, 0x90, 0x90 };
    inline unsigned char original_actor_instructions[12];
    inline unsigned char original_camera_instructions[8];
};

void c_cache::setup_camera()
{
    const auto patch_address = g_imagebase + offset::camera_patch_address;
    const auto codecave_address = g_imagebase + offset::code_cave_one;

    int rel_offset = static_cast<int>(codecave_address - (patch_address + 7));

    unsigned char patch[8] = {
        0x48, 0x89, 0x15, // camera pointer should always be stored inside RDX register (even on all builds)
        static_cast<unsigned char>(rel_offset & 0xFF),
        static_cast<unsigned char>((rel_offset >> 8) & 0xFF),
        static_cast<unsigned char>((rel_offset >> 16) & 0xFF),
        static_cast<unsigned char>((rel_offset >> 24) & 0xFF),
        0x90
    };

    hv::write_virt_mem(hv::g_cr3, (void*)patch_address, patch, sizeof(patch));
}

void c_cache::setup_actor()
{
    const auto patch_address = g_imagebase + offset::actor_patch_address;
    const auto codecave_address = g_imagebase + offset::code_cave_two;

    int rel_offset = static_cast<int>(codecave_address - (patch_address + 7));

    unsigned char patch[12] = {
        offset::actor_mov_instruction[0],  offset::actor_mov_instruction[1],  offset::actor_mov_instruction[2],
        0x90,
        0x90,
        0x90,
        0x90,
        0x90, 0x90, 0x90, 0x90, 0x90
    };

    hv::write_virt_mem(hv::g_cr3, (void*)patch_address, patch, sizeof(patch));
}

void c_cache::setup_cache()
{
    hv::read_virt_mem(offset::original_camera_instructions, (void*)(g_imagebase + offset::camera_patch_address), 8);
    setup_camera();
	// we store the original instructions so we can restore them later if needed
    hv::read_virt_mem(offset::original_actor_instructions, (void*)(g_imagebase + offset::actor_patch_address), 12);
    setup_actor();

    std::vector<c_actor*> temp_list;
	std::unordered_set<c_actor*> actor_set;

	    // we dont sleep in this loop due to the fact that we are moving the register into a codecave, the function for actor gets called very fast and pointers changes alot. 
	    // If we sleep we may miss an actor pointer leading to it not drawing all players
    auto last_new_pointer_time = std::chrono::steady_clock::now();

	uintptr_t camera_code_cave_phys = hv::get_physical_address(hv::g_cr3, (void*)(g_imagebase + offset::code_cave_one));
	//uintptr_t actor_code_cave_phys = hv::get_physical_address(hv::g_cr3, (void*)(g_imagebase + offset::code_cave_two));

    while (true)
    {
        unsigned long long camera_addr = hv::read_phys_mem<unsigned long long>(camera_code_cave_phys);
        if (!camera_addr)
            continue;

        camera = reinterpret_cast<c_camera*>(camera_addr);

        //unsigned long long entity_addr = hv::read_phys_mem<unsigned long long>(actor_code_cave_phys);


        uint64_t temp_actor_list[400];
        size_t found_actors = hv::get_actor_set((uint64_t)temp_actor_list, 3200);


	    {
			std::lock_guard lock(c_overlay::actor_list_mutex);
            actor_list.clear();
		    for (int i = 0; i < found_actors / 8; i++)
		    {
		    	actor_list.push_back((c_actor*)temp_actor_list[i]);
		    }
	    }
        Sleep(2000);
    }
}