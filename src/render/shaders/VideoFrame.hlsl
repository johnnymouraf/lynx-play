// Draws a YUV 4:2:0 picture (NV12 or P010) over the whole viewport as RGB.

cbuffer Conversion : register(b0)
{
    // Rows that turn the samples (Y, Cb, Cr, 1) into gamma-encoded RGB.
    float4 redRow;
    float4 greenRow;
    float4 blueRow;
    // The visible picture's share of the texture, which may be padded.
    float2 visibleExtent;
};

// One slice each: the view picks the frame's slice of a texture array.
Texture2DArray<float> lumaPlane : register(t0);
Texture2DArray<float2> chromaPlane : register(t1);
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
    output.uv = corner * visibleExtent;

    return output;
}

float4 pixelMain(Interpolants input) : SV_Target
{
    const float3 location = float3(input.uv, 0.0);
    const float4 ycbcr = float4(lumaPlane.Sample(bilinear, location),
                                chromaPlane.Sample(bilinear, location),
                                1.0);

    const float3 rgb = float3(dot(redRow, ycbcr), dot(greenRow, ycbcr), dot(blueRow, ycbcr));

    return float4(saturate(rgb), 1.0);
}
