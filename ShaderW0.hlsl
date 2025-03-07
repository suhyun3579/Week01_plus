cbuffer constants : register(b0)
{
	float3 Offset;
	float Radius;
}

struct VS_INPUT
{
	float4 position : POSITION;	// Input position from vertex buffer
	float4 color : COLOR;	// Input color from vertex buffer
};

struct PS_INPUT
{
	float4 position : SV_POSITION;	// Transformed position to pass to the pixel shader
	float4 color : COLOR;	// Color to pass to the pixel shader
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;

	float3 scaledPos = input.position * Radius;
	output.position = float4(scaledPos + Offset, 1.0f);
	output.color = input.color;

	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}