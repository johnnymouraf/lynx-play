// Draws a converted RGB picture over the whole viewport, scaling it.

Texture2D<float4> picture : register(t0);
SamplerState bilinear : register(s0);

struct Interpolants
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// A triangle twice the viewport's size covers it with a single primitive.
Interpolants vertexMain(uint vertexId : SV_VertexID)
{
    const float2 corner = float2(float((vertexId << 1) & 2), float(vertexId & 2));

    Interpolants output;
    output.position = float4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
    output.uv = corner;

    return output;
}

float4 pixelMain(Interpolants input) : SV_Target
{
    return float4(picture.Sample(bilinear, input.uv).rgb, 1.0);
}
