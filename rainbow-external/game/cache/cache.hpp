#pragma once

// Macros to run code every x interval
#define RUN_EVERY_START(interval) \
    do { \
        static auto last_time = std::chrono::high_resolution_clock::now() - 99999s; \
        auto current_time = std::chrono::high_resolution_clock::now(); \
        auto elapsed = current_time - last_time; \
        if (elapsed >= (interval)) {

#define RUN_EVERY_END \
            last_time = current_time; \
        } \
    } while(0)

class c_cache
{
public:
	static void setup_cache();
	static void setup_camera();
	static void setup_actor();

	inline static std::vector<c_actor*> actor_list;
	inline static c_camera* camera;
};

inline c_cache cache;