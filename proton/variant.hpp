#pragma once
#include "../Packet.h"

#include "vector.hpp"
#include "../enet/dirent.h"

#include <variant>
#include <iostream>
#include <vector>
#include <type_traits>
#include <string_view>
#include <memory>

namespace {
	using VariantTypes = std::variant<int, float, std::uint32_t, std::string, proton::Vector2, proton::Vector3>;
}

namespace proton {
	class Variant {
	private:
		std::vector<VariantTypes> variants_;
	public:
		Variant(char const* function) : variants_ { std::string { function } } {}

		template<typename... Args>
		Variant& push(Args const&... args) {
			((variants_.emplace_back(args)), ...);
			return *this;
		}

		ENetPacket* pack(int const& netId = -1, int const& delay = 0) const {
			gamepacket_t packet(delay, netId);

			for (VariantTypes const& variant : variants_) {
				switch (variant.index()) {
					case 0: packet.Insert(std::get<0>(variant));
						break;
					case 1: packet.Insert(std::get<1>(variant));
						break;
					case 2: packet.Insert(std::get<2>(variant));
						break;
					case 3: packet.Insert(std::get<3>(variant));
						break;
					case 4: {
						proton::Vector2 v = std::get<4>(variant);
						packet.Insert(v.get_x(), v.get_y());
					} break;
					case 5: {
						proton::Vector3 v = std::get<5>(variant);
						packet.Insert(v.get_x(), v.get_y(), v.get_x());
					} break;
					default: break;
				}
			}

			return packet.get_packet();
		}
	};
}