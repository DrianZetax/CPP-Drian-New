#pragma once
namespace proton {
	class Vector2 {
	private:
		float x, y;
	public:
		Vector2(float const& x, float const& y) : x(x), y(y) {}

		float get_x() const { return x; }
		float get_y() const { return y; }
	};
	class Vector3 {
	private:
		float x, y, z;
	public:
		Vector3(float const& x, float const& y, float const& z) : x(x), y(y), z(z) {}

		float get_x() const { return x; }
		float get_y() const { return y; }
		float get_z() const { return z; }
	};
}