#pragma once

class c_camera
{
public:
	unsigned long long get_instance();

	matrix4_t get_matrix();
	vector2_t get_resolution();
	vector3_t get_position();

	vector2_t get_screen_position(const vector3_t& position);
};

class c_actor
{
public:

	c_actor(unsigned long long instance, std::pair<vector3_t, vector3_t> actor_bounds) : instance_(instance), actor_bounds_(actor_bounds) {}

	unsigned long long get_instance();

	enum class actor_status
	{
		DEAD_1,
		DEAD_2,
		TEAM,
		LOCAL,
		VALID
	};

	actor_status get_actor_status();
	bool is_actor_player();

	vector3_t get_actor_position();
	std::pair<vector3_t, vector3_t> get_actor_bounds();
	void set_actor_bounds(std::pair<vector3_t, vector3_t> actor_bounds)
	{
		actor_bounds_ = actor_bounds;
	}
private:
	std::pair<vector3_t, vector3_t> actor_bounds_;
	unsigned long long instance_;
};