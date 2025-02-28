#pragma once

#include <iostream>
#include <ShObjIdl_core.h>

typedef unsigned char Byte;
typedef unsigned int uint;

#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))


std::string ToUtf8(const std::wstring& str);
//Not commutative
unsigned int HashCombine(unsigned int A, unsigned int C);
size_t HashCombine2(size_t A, size_t C);



struct Float3
{
	Float3() :
		x(0.0), y(0.0), z(0.0) {}
	Float3(float a) :
		x(a), y(a), z(a) {}
	Float3(float _x, float _y, float _z) :
		x(_x), y(_y), z(_z) {}

	float x;
	float y;
	float z;

	float& operator[](unsigned int i)
	{
		return (i == 0 ? x : (i == 1 ? y : z));
	}
	float& operator[](int i)
	{
		return (i == 0 ? x : (i == 1 ? y : z));
	}
	const float& operator[](unsigned int i) const
	{
		return (i == 0 ? x : (i == 1 ? y : z));
	}
	const float& operator[](int i) const
	{
		return (i == 0 ? x : (i == 1 ? y : z));
	}

	friend Float3 operator-(const Float3& a, const Float3& b);
	friend Float3 operator+(const Float3& a, const Float3& b);
	friend Float3 operator*(const Float3& a, const double b);
	friend Float3 operator*(const Float3& a, const Float3& b);
	friend Float3 operator/(const Float3& a, const double b);
	friend Float3 Normalize(const Float3& a);
	friend float Length(const Float3& a);
	friend Float3 Cross(const Float3& a, const Float3& b);
	friend float Dot(const Float3& a, const Float3& b);
};

Float3 CalculateNormal(Float3& x, Float3& y, Float3& z);


struct Float2
{
	Float2() :
		x(0.0), y(0.0) {}
	Float2(float a) :
		x(a), y(a) {}
	Float2(float _x, float _y) :
		x(_x), y(_y) {}

	float x;
	float y;

	float& operator[](unsigned int i)
	{
		return (i == 0 ? x : y);
	}
	float& operator[](int i)
	{
		return (i == 0 ? x : y);
	}
	const float& operator[](unsigned int i) const
	{
		return (i == 0 ? x : y);
	}
	const float& operator[](int i) const
	{
		return (i == 0 ? x : y);
	}

	friend Float2 operator-(const Float2& a, const Float2& b);
	friend Float2 operator+(const Float2& a, const Float2& b);
	friend Float2 operator*(const Float2& a, const double b);
	friend Float2 operator*(const Float2& a, const Float2& b);
	friend Float2 operator/(const Float2& a, const double b);
	friend Float2 Normalize(const Float2& a);
	friend float Length(const Float2& a);
	friend float Dot(const Float2& a, const Float2& b);
};



struct Uint3
{
	Uint3(uint a = 0) :
		x(a), y(a), z(a) {}
	Uint3(uint _x, uint _y, uint _z) :
		x(_x), y(_y), z(_z) {}
	Uint3(Float3 a) :
		x(a.x), y(a.y), z(a.z)
	{}

	uint x;
	uint y;
	uint z;
};

inline int BytesToUnsignedIntegerLittleEndian(Byte* Src, size_t Offset)
{
	return static_cast<int>(static_cast<Byte>(Src[Offset]) |
		static_cast<Byte>(Src[Offset + 1]) << 8 |
		static_cast<Byte>(Src[Offset + 2]) << 16 |
		static_cast<Byte>(Src[Offset + 3]) << 24);
}

inline float BytesToFloatLittleEndian(Byte* Src, size_t Offset)
{
	uint32_t value = static_cast<uint32_t>(Src[Offset]) |
		static_cast<uint32_t>(Src[Offset + 1]) << 8 |
		static_cast<uint32_t>(Src[Offset + 2]) << 16 |
		static_cast<uint32_t>(Src[Offset + 3]) << 24;

	return *reinterpret_cast<float*>(&value);
}

inline std::string BytesToASCIIString(Byte* Src, size_t Offset, int Length)
{
	char* Buffer = new char[size_t(Length) + 1];
	memcpy(Buffer, Src + Offset, Length);
	Buffer[Length] = '\0';

	std::string Str = Buffer;
	delete[] Buffer;
	return std::move(Str);
}


inline void WriteUnsignedIntegerToBytesLittleEndian(Byte* Src, size_t* Offset, uint Value)
{
	Src[*Offset] = Value & 0x000000ff;
	Src[*Offset + 1] = (Value & 0x0000ff00) >> 8;
	Src[*Offset + 2] = (Value & 0x00ff0000) >> 16;
	Src[*Offset + 3] = (Value & 0xff000000) >> 24;
	*Offset += 4;
}

inline void WriteUnsignedInteger16ToBytesLittleEndian(Byte* Src, size_t* Offset, std::uint16_t Value)
{
	Src[*Offset] = Value & 0x000000ff;
	Src[*Offset + 1] = (Value & 0x0000ff00) >> 8;
	*Offset += 2;
}


inline void WriteASCIIStringToBytes(Byte* Dst, size_t* Offset, std::string& Value)
{
	memcpy(Dst + *Offset, Value.c_str(), Value.length());
	*Offset += Value.length();
}

inline void WriteFloatToBytesLittleEndian(Byte* Src, size_t* Offset, float Value)
{
	uint* T = (uint*)&Value;
	uint V = *T;
	WriteUnsignedIntegerToBytesLittleEndian(Src, Offset, V);
}



static uint PackFloatsToUint(float a, float b)
{
	uint aScaled = a * 65535.0f;
	uint bScaled = b * 65535.0f;
	return (aScaled << 16) | (bScaled & 0xFFFF);
}

static void UnpackUintToFloats(uint v, float* a, float* b)
{
	uint uintInput = v;
	*a = (uintInput >> 16) / 65535.0f;
	*b = (uintInput & 0xFFFF) / 65535.0f;
}
/*
* Precision : 0.00005f
*/
static Uint3 EncodeNormalsToUint3(Float3 N1, Float3 N2)
{
	Uint3 Result = 0;
	Result.x = PackFloatsToUint(N1.x * 0.5f + 0.5f, N1.y * 0.5f + 0.5f);
	Result.y = PackFloatsToUint(N1.z * 0.5f + 0.5f, N2.x * 0.5f + 0.5f);
	Result.z = PackFloatsToUint(N2.y * 0.5f + 0.5f, N2.z * 0.5f + 0.5f);

	return Result;
}

static void DecodeUint3ToNormals(Uint3 V, Float3* N1, Float3* N2)
{
	Float3 R1 = 0.0f;
	Float3 R2 = 0.0f;
	UnpackUintToFloats(V.x, &R1.x, &R1.y);
	UnpackUintToFloats(V.y, &R1.z, &R2.x);
	UnpackUintToFloats(V.z, &R2.y, &R2.z);

	*N1 = R1 * 2.0f - 1.0f;
	*N2 = R2 * 2.0f - 1.0f;
}




struct BoundingBox
{
	BoundingBox() :
		Center(0.0),
		HalfLength(1.0),
		Min(-1.0),
		Max(1.0)
	{}

	void Resize(Float3 Point)
	{
		Min.x = MIN(Point.x, Min.x);
		Min.y = MIN(Point.y, Min.y);
		Min.z = MIN(Point.z, Min.z);

		Max.x = MAX(Point.x, Max.x);
		Max.y = MAX(Point.y, Max.y);
		Max.z = MAX(Point.z, Max.z);

		HalfLength = (Max - Min) * 0.5;
		Center = Min + HalfLength;
	}

	void Resize(BoundingBox& Box)
	{
		Min.x = MIN(Box.Min.x, Min.x);
		Min.y = MIN(Box.Min.y, Min.y);
		Min.z = MIN(Box.Min.z, Min.z);

		Max.x = MAX(Box.Max.x, Max.x);
		Max.y = MAX(Box.Max.y, Max.y);
		Max.z = MAX(Box.Max.z, Max.z);

		HalfLength = (Max - Min) * 0.5;
		Center = Min + HalfLength;
	}

	void Clear()
	{
		Center = Float3(0.0);
		HalfLength = Float3(1.0);
		Min = Float3(-1.0);
		Max = Float3(1.0);
	}

	Float3 Center;
	Float3 HalfLength;
	Float3 Min;
	Float3 Max;
};
