#pragma once



static const char* InternalShader =
"\
#ifndef DRAW_WIREFRAME\n\
#define DRAW_WIREFRAME 0\n\
#endif\n\
#ifndef DRAW_NORMAL\n\
#define DRAW_NORMAL 0\n\
#endif\n\
\n\
cbuffer ConstantBuffer : register(b0)\n\
{\n\
	float4x4 WorldViewProjection;\n\
};\n\
\n\
struct VSInput\n\
{\n\
	float3 position : POSITION;\n\
	float3 normal : NORMAL;\n\
	float4 color : COLOR0;\n\
};\n\
struct VSOutput\n\
{\n\
	float4 position : SV_POSITION;\n\
	float3 normal : TEXCOORD0;\n\
	float3 color : TEXCOORD1;\n\
};\n\
\n\
VSOutput VSMain(VSInput input)\n\
{\n\
	VSOutput output = (VSOutput)0;\n\
\n\
	float4 positionLocal = float4(input.position, 1.0f);\n\
	output.normal = normalize(input.normal);\n\
#if DRAW_WIREFRAME\n\
	positionLocal.xyz = positionLocal.xyz + output.normal.xyz * 0.0001f;\n\
#endif\n\
	output.position = mul(positionLocal, WorldViewProjection);\n\
\n\
	output.color = float3(0.0f, 0.0f, 0.0f);\n\
#if !DRAW_WIREFRAME && !DRAW_NORMAL\n\
	float3 NdL = dot(output.normal, float3(-1.0, 1.0, 1.0)) * 0.5f + 0.5f;\n\
	output.color = lerp(float3(0.2f, 0.2f, 0.3f), float3(0.3f, 0.3f, 0.8f), NdL);\n\
#endif\n\
\n\
	return output;\n\
}\n\
\n\
float4 PSMain(VSOutput input) : SV_TARGET0\n\
{\n\
	float4 color = float4(0.0f,0.0f,0.0f,0.0f);\n\
#if DRAW_WIREFRAME\n\
	color = float4(0.8f,0.8f,0.9f, 1.0f);\n\
#elif DRAW_NORMAL\n\
	color = float4(0.2f,0.8f,0.2f, 1.0f);\n\
#else\n\
	color = float4(input.color,1.0f);\n\
#endif\n\
\n\
	return color;\n\
}\n\
";