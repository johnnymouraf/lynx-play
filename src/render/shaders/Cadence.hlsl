// Compares the current and previous pictures in 4x4 luma blocks: counts the
// blocks that changed and sums their absolute differences, for the cadence
// state machine.

cbuffer Cadence : register(b0)
{
    // The vector grid: one block per cell.
    uint2 blockExtent;
    // The visible picture, which may not be a multiple of four.
    uint2 pictureExtent;
    // Sum of absolute differences, over the 16 samples, of one 8-bit level.
    float levelThreshold;
    uint3 padding;
};

Texture2DArray<float4> currentLuma : register(t0);
Texture2DArray<float4> previousLuma : register(t1);
RWStructuredBuffer<uint> result : register(u0);

groupshared uint groupChanged;
groupshared uint groupSad;

[numthreads(64, 1, 1)]
void main(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex,
          uint3 dispatch : SV_DispatchThreadID)
{
    if (groupIndex == 0)
    {
        groupChanged = 0;
        groupSad = 0;

        if (groupId.x == 0)
        {
            result[1] = blockExtent.x * blockExtent.y;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    const uint block = dispatch.x;
    uint blockSad = 0;
    bool changed = false;

    if (block < blockExtent.x * blockExtent.y)
    {
        const uint2 cell = uint2(block % blockExtent.x, block / blockExtent.x);
        const uint2 origin = cell * 4;
        float sum = 0.0;

        [unroll]
        for (uint y = 0; y < 4; ++y)
        {
            [unroll]
            for (uint x = 0; x < 4; ++x)
            {
                const uint2 pixel = min(origin + uint2(x, y), pictureExtent - uint2(1, 1));
                const int4 location = int4(int2(pixel), 0, 0);
                sum += abs(currentLuma.Load(location).x - previousLuma.Load(location).x);
            }
        }

        // Both 8-bit and P010 luma land on the same 8-bit-level scale.
        const float levels = sum * 255.0;
        blockSad = uint(min(levels + 0.5, 4080.0));
        changed = levels > levelThreshold * 16.0;
    }

    if (changed)
    {
        InterlockedAdd(groupChanged, 1);
    }

    if (blockSad > 0)
    {
        InterlockedAdd(groupSad, blockSad);
    }

    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        if (groupChanged > 0)
        {
            InterlockedAdd(result[0], groupChanged);
        }

        if (groupSad > 0)
        {
            InterlockedAdd(result[2], groupSad);
        }
    }
}
