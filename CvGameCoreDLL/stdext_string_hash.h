#pragma once

#include "hash_map"
#include <string_view>

namespace utils::stdext
{
	struct string_traits {
		using is_transparent = void;
		enum { bucket_size = 4, min_buckets = 8 };

		std::size_t operator()(std::string_view s) const {
			return ::stdext::hash_value(s);
		}
		bool operator()(std::string_view a, std::string_view b) const {
			return a < b;
		}
	};
}
