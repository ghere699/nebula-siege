#include "includes.hpp"
#include <unordered_set>
#include "Zydis\Zydis.h"


// this namespace was generated from my dumper which gathers and auto generates shellcode for each build
// I will provide my current build for the latest r6s update (25 June 2025)
// you can go into IDA and find these addresses and make your own sigs for your build...

struct bounds_struct {
	uint64_t bounds_ptr;

	float bounds_max1;
	float bounds_max2;
	float bounds_max3;
	
	float bounds_min1;
	float bounds_min2;
	float bounds_min3;

};

struct InstructionInfo {
	ZyanU64 runtime_address;
	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
	std::string text; // Still useful for printing
};

bool disassemble_backward(
	std::vector<InstructionInfo>& result,
	const ZyanU8* code_buffer,
	ZyanUSize buffer_size,
	ZyanU64 base_address,
	ZyanU64 known_good_address,
	ZyanUSize desired_size)
{
	ZydisDecoder decoder;
	ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

	ZydisFormatter formatter;
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

	const ZyanUSize known_good_offset = known_good_address - base_address;
	ZyanUSize start_offset = 0;
	if (known_good_offset > desired_size + 15) { // 15 is max instruction length
		start_offset = known_good_offset - (desired_size + 15);
	}

	for (ZyanUSize i = start_offset; i < known_good_offset; ++i)
	{
		ZyanUSize current_offset = i;
		std::vector<InstructionInfo> potential_sequence;
		bool sequence_valid = true;

		while (current_offset < known_good_offset)
		{
			InstructionInfo info;
			if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, code_buffer + current_offset, buffer_size - current_offset, &info.instruction, info.operands))) {
				sequence_valid = false;
				break;
			}

			info.runtime_address = base_address + current_offset;
			char buffer[256];
			ZydisFormatterFormatInstruction(&formatter, &info.instruction, info.operands, info.instruction.operand_count, buffer, sizeof(buffer), info.runtime_address, ZYAN_NULL);
			info.text = buffer;
			potential_sequence.push_back(info);

			current_offset += info.instruction.length;
		}

		if (sequence_valid && current_offset == known_good_offset) {
			result = potential_sequence;
			return true;
		}
	}
	return false;
}

namespace offset
{
	inline constexpr unsigned long long camera_patch_address = 0xE29B8F4;
	inline constexpr unsigned long long actor_patch_address = 0x21FA88A;
	inline constexpr unsigned long long bounds_max_patch_address = 0xAC74964;
	inline constexpr unsigned long long bounds_min_patch_address = 0xAC73926;
	inline constexpr unsigned long long bone_patch_address = 0xCA6E1A7;
	inline constexpr unsigned long long code_cave_one = 0x1178800D;
	inline constexpr unsigned long long code_cave_two = 0x128C10B0;
	inline constexpr unsigned char actor_mov_instruction[3] = { 0x90, 0x90, 0x90 };
	inline unsigned char original_actor_instructions[3] = { 0x48, 0x85, 0xC9 };
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
	//setup_actor();

	const ZyanU64 base_address = g_imagebase + offset::actor_patch_address - 1000;
	const ZyanU64 known_good_address = g_imagebase + offset::actor_patch_address; // The address of 'call rax'
	const ZyanUSize desired_scan_size = 1000;    // Scan about 20 bytes back

	uint8_t actor_asm_buffer[1000];

	hv::read_virt_mem(actor_asm_buffer, (void*)(g_imagebase + offset::actor_patch_address - 1000), 1000);

	std::vector<InstructionInfo> instructions;
	if (!disassemble_backward(instructions, actor_asm_buffer, sizeof(actor_asm_buffer), base_address, known_good_address, desired_scan_size)) {
		std::cerr << "Failed to find a valid instruction sequence." << std::endl;
		return;
	}

	// --- PATTERN MATCHING LOGIC ---
	const InstructionInfo* found_mov_instruction = nullptr;

	const char* reg_name {};
	bool found_pattern = false;

	// Iterate backwards through the correctly disassembled instructions
	for (auto it = instructions.crbegin(); it != instructions.crend(); ++it) {
		const auto& current_instruction_info = *it;

		// STATE 1: We have found our MOV, now check if the instruction before it is SUB RSP, ...
		if (found_mov_instruction) {
			// Check for SUB mnemonic
			if (current_instruction_info.instruction.mnemonic == ZYDIS_MNEMONIC_SUB &&
				current_instruction_info.instruction.operand_count > 0) {
				// Check that the first operand is the RSP register
				const auto& dest_op = current_instruction_info.operands[0];
				if (dest_op.type == ZYDIS_OPERAND_TYPE_REGISTER && dest_op.reg.value == ZYDIS_REGISTER_RSP) {
					// *** SUCCESS: PATTERN FOUND ***
					std::cout << "\nPattern found!" << std::endl;
					std::cout << "  - Found: " << current_instruction_info.text << std::endl;
					std::cout << "  - Followed by: " << found_mov_instruction->text << std::endl;

					// Now, get the first operand of the MOV instruction we saved
					if (found_mov_instruction->instruction.operand_count > 0) {
						const auto& mov_first_op = found_mov_instruction->operands[0];
						if (mov_first_op.type == ZYDIS_OPERAND_TYPE_REGISTER) {
							reg_name = ZydisRegisterGetString(mov_first_op.reg.value);
							std::cout << "\n>> The first register of the MOV instruction is: " << reg_name << std::endl;
							found_pattern = true;
						}
						else {
							std::cout << "\n>> The first operand of the MOV is not a register." << std::endl;
						}
					}
					break; // Exit after finding the first match
				}
			}
			// If it's not the SUB RSP we're looking for, the pattern is broken. Reset.
			found_mov_instruction = nullptr;
		}

		// STATE 0: We are looking for the first occurrence of a MOV instruction
		if (current_instruction_info.instruction.mnemonic == ZYDIS_MNEMONIC_MOV) {
			found_mov_instruction = &current_instruction_info;
			// Continue to the next iteration to check the instruction *before* this one
			continue;
		}
	}

	if (!found_pattern)
	{
		std::cout << "\nPattern not found in the disassembled code." << std::endl;
		return;
	}

	uint64_t reg_nameu64 {0};

	strcpy_s((char*)&reg_nameu64, 8, reg_name);

	hv::for_each_cpu([&](uint32_t)
		{
			hv::r6hook(hv::g_cr3, g_imagebase + offset::actor_patch_address, reg_nameu64);
			hv::bounds_hook(hv::g_cr3, g_imagebase + offset::bounds_min_patch_address, g_imagebase + offset::bounds_max_patch_address);
		});


	uintptr_t camera_code_cave_phys = hv::get_physical_address(hv::g_cr3, (void*)(g_imagebase + offset::code_cave_one));
	//uintptr_t actor_code_cave_phys = hv::get_physical_address(hv::g_cr3, (void*)(g_imagebase + offset::code_cave_two));

	Sleep(2000);
	unsigned long long camera_addr = hv::read_phys_mem<unsigned long long>(camera_code_cave_phys);
	hv::write_virt_mem(hv::g_cr3, (void*)(g_imagebase + offset::camera_patch_address), offset::original_camera_instructions, 8);

	while (true)
	{
		if (!camera_addr)
			continue;

		camera = reinterpret_cast<c_camera*>(camera_addr);

		//unsigned long long entity_addr = hv::read_phys_mem<unsigned long long>(actor_code_cave_phys);


		uint64_t temp_actor_list[400]{};
		bounds_struct temp_bounds_list[100]{};
		size_t found_actors = hv::get_actor_set((uint64_t)temp_actor_list, 3200);
		size_t found_bounds = hv::get_bounds((uint64_t)temp_bounds_list, sizeof(bounds_struct) * 100);

		{
			std::lock_guard lock(c_overlay::actor_list_mutex);
			actor_list.clear();
			for (int i = 0; i < found_actors / 8; i++)
			{
				std::pair<vector3_t, vector3_t> bounds_pair = { 0, 0 };
				actor_list.emplace_back(temp_actor_list[i], bounds_pair);
				for (int j = 0; j < found_bounds / sizeof(bounds_struct); j++)
				{
					if (temp_bounds_list[j].bounds_ptr == (temp_actor_list[i] + 0x80))
					{
						vector3_t min{ temp_bounds_list[j].bounds_min1, temp_bounds_list[j].bounds_min2, temp_bounds_list[j].bounds_min3 };
						vector3_t max{ temp_bounds_list[j].bounds_max1, temp_bounds_list[j].bounds_max2, temp_bounds_list[j].bounds_max3 };
						bounds_pair = { min, max };
						actor_list[i].set_actor_bounds(bounds_pair);
						
					}
				}
			}
		}
		hv::cleanup_bounds();
		hv::cleanup_actor();
		Sleep(2000);
	}
}