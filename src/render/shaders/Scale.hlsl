// Draws a converted RGB picture over the whole viewport, scaling it. A PQ
// picture going to an SDR surface is tone-mapped on the way.

cbuffer Scaling : register(b0)
{
    // 1 when the picture is PQ and the surface is SDR.
    uint toneMap;
    // The stream's peak luminance, and the SDR white the tone map targets.
    float contentPeakNits;
    float sdrWhiteNits;
    float reserved;
};

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

// SMPTE ST.2084: a PQ code to light in nits.
float3 pqToNits(float3 code)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;

    const float3 e = pow(max(code, 0.0), 1.0 / m2);
    return 10000.0 * pow(max(e - c1, 0.0) / (c2 - c3 * e), 1.0 / m1);
}

// Linear BT.2020 primaries to linear BT.709 primaries.
float3 bt2020ToBt709(float3 color)
{
    return float3(dot(color, float3(1.6605, -0.5876, -0.0728)),
                  dot(color, float3(-0.1246, 1.1329, -0.0083)),
                  dot(color, float3(-0.0182, -0.1006, 1.1188)));
}

float3 toneMapToSdr(float3 pq)
{
    const float3 nits = pqToNits(pq);
    const float3 linearColor = max(bt2020ToBt709(nits), 0.0) / max(sdrWhiteNits, 1.0);
    const float peak = max(contentPeakNits, sdrWhiteNits) / max(sdrWhiteNits, 1.0);
    const float3 mapped =
        linearColor * (1.0 + linearColor / (peak * peak)) / (1.0 + linearColor);

    return pow(saturate(mapped), 1.0 / 2.2);
}

float4 pixelMain(Interpolants input) : SV_Target
{
    const float3 rgb = picture.Sample(bilinear, input.uv).rgb;

    return float4(toneMap != 0 ? toneMapToSdr(rgb) : rgb, 1.0);
}
