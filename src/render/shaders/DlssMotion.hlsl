// Densifies the optical-flow engine's 4x4 grid into the per-pixel
// R16G16_FLOAT motion vectors DLSS samples: the cell's vector, S10.5 fixed
// point converted to pixels. The engine's vectors already run from the
// current picture to the previous one, which is the direction DLSS expects.

cbuffer Motion : register(b0)
{
    uint2 pictureExtent;
    uint2 gridExtent;
    uint gridSize;
    uint3 padding;
};

Texture2D<int2> flowGrid : register(t0);
RWTexture2D<float2> motion : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatch : SV_DispatchThreadID)
{
    if (dispatch.x >= pictureExtent.x || dispatch.y >= pictureExtent.y)
    {
        return;
    }

    const uint2 pixel = dispatch.xy;
    const uint2 cell = min(pixel / gridSize, gridExtent - uint2(1, 1));

    motion[pixel] = float2(flowGrid.Load(int3(int2(cell), 0))) / 32.0;
}
