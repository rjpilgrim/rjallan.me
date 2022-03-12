#pragma once
#include <array>
#include <vector>
#include "thread_pool.hpp"

class SignalBlock {
	std::array buffer;
	unsigned int history;
	std::vector<std::reference_wrapper<std::array>> output;
	thread_pool & pool;
}