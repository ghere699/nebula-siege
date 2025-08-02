#include "includes.hpp"

// this namespace was generated from my dumper which gathers and auto generates shellcode for each build
// I will provide my current build for the latest r6s update (25 June 2025)
// you can go into IDA and find these addresses and make your own sigs for your build...

namespace offset
{
    inline constexpr unsigned long long camera_patch_address = 0xB8BE3C3;
    inline constexpr unsigned long long actor_patch_address = 0x4DEFFF8;
    inline constexpr unsigned long long code_cave_one = 0x128C1000;
    inline constexpr unsigned long long code_cave_two = 0x128C10B0;
    inline constexpr unsigned char actor_mov_instruction[3] = { 0x4C, 0x89, 0x2D };
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
        static_cast<unsigned char>(rel_offset & 0xFF),
        static_cast<unsigned char>((rel_offset >> 8) & 0xFF),
        static_cast<unsigned char>((rel_offset >> 16) & 0xFF),
        static_cast<unsigned char>((rel_offset >> 24) & 0xFF),
        0x90, 0x90, 0x90, 0x90, 0x90
    };

    hv::write_virt_mem(hv::g_cr3, (void*)patch_address, patch, sizeof(patch));
}

void c_cache::setup_cache()
{
    setup_camera();
    setup_actor();

    std::vector<c_actor*> temp_list;

    // we dont sleep in this loop due to the fact that we are moving the register into a codecave, the function for actor gets called very fast and pointers changes alot. 
    // If we sleep we may miss an actor pointer leading to it not drawing all players

    while (true)
    {
        unsigned long long camera_addr = hv::read_virt_mem<unsigned long long>(g_imagebase + offset::code_cave_one);
        if (!camera_addr)
            continue;

        camera = reinterpret_cast<c_camera*>(camera_addr);

        unsigned long long entity_addr = hv::read_virt_mem<unsigned long long>(g_imagebase + offset::code_cave_two);
        if (!entity_addr)
            continue;

        c_actor* actor = reinterpret_cast<c_actor*>(entity_addr);

        if (std::find(temp_list.begin(), temp_list.end(), actor) != temp_list.end())
            continue;

        temp_list.push_back(actor);
        actor_list = temp_list;
    }
}