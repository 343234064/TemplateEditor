#include "Utils.h"


using namespace std;

std::string ToUtf8(const std::wstring& str)
{
	std::string ret;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
	if (len > 0)
	{
		ret.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), &ret[0], len, NULL, NULL);
	}
	return ret;
}

unsigned int HashCombine(unsigned int A, unsigned int C)
{
	unsigned int B = 0x9e3779b9;
	A += B;

	A -= B; A -= C; A ^= (C >> 13);
	B -= C; B -= A; B ^= (A << 8);
	C -= A; C -= B; C ^= (B >> 13);
	A -= B; A -= C; A ^= (C >> 12);
	B -= C; B -= A; B ^= (A << 16);
	C -= A; C -= B; C ^= (B >> 5);
	A -= B; A -= C; A ^= (C >> 3);
	B -= C; B -= A; B ^= (A << 10);
	C -= A; C -= B; C ^= (B >> 15);

	return C;
}

size_t HashCombine2(size_t A, size_t C)
{
	size_t value = A;
	value ^= C + 0x9e3779b9 + (A << 6) + (A >> 2);
	return value;
}



Float3 CalculateNormal(Float3& x, Float3& y, Float3& z)
{
	Float3 U = y - x;
	Float3 V = z - x;

	Float3 Normal = Float3(0.0f, 0.0f, 0.0f);

	Normal = Cross(U, V);
	Normal = Normalize(Normal);

	return Normal;
}

Float3 operator-(const Float3& a, const Float3& b)
{
	return Float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Float3 operator+(const Float3& a, const Float3& b)
{
	return Float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Float3 operator*(const Float3& a, const double b)
{
	return Float3(a.x * b, a.y * b, a.z * b);
}

Float3 operator*(const Float3& a, const Float3& b)
{
	return Float3(a.x * b.x, a.y * b.y, a.z * b.z);
}

Float3 operator/(const Float3& a, const double b)
{
	return Float3(a.x / b, a.y / b, a.z / b);
}

Float3 Normalize(const Float3& a)
{
	float MagInv = 1.0f / Length(a);
	return a * MagInv;
}

float Length(const Float3& a)
{
	return MAX(0.000001f, sqrt(a.x * a.x + a.y * a.y + a.z * a.z));
}

Float3 Cross(const Float3& a, const Float3& b)
{
	return Float3(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	);
}

float Dot(const Float3& a, const Float3& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}







Float2 operator-(const Float2& a, const Float2& b)
{
	return Float2(a.x - b.x, a.y - b.y);
}

Float2 operator+(const Float2& a, const Float2& b)
{
	return Float2(a.x + b.x, a.y + b.y);
}

Float2 operator*(const Float2& a, const double b)
{
	return Float2(a.x * b, a.y * b);
}

Float2 operator*(const Float2& a, const Float2& b)
{
	return Float2(a.x * b.x, a.y * b.y);
}

Float2 operator/(const Float2& a, const double b)
{
	return Float2(a.x / b, a.y / b);
}

Float2 Normalize(const Float2& a)
{
	float MagInv = 1.0f / Length(a);
	return a * MagInv;
}

float Length(const Float2& a)
{
	return MAX(0.000001f, sqrt(a.x * a.x + a.y * a.y));
}

float Dot(const Float2& a, const Float2& b)
{
	return a.x * b.x + a.y * b.y;
}