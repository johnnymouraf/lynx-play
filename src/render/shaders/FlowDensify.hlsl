// NVIDIA's optical flow is a coarse grid of S10.5 vectors over the source
// picture; DLSS Frame Generation wants one current-to-previous vector per
// back-buffer pixel, in pixels at that resolution. The source is drawn
// letterboxed into the back buffer, so the shader maps through the fit
// rectangle, blends the grid bilinearly, scales the vectors with the fit and
// leaves the bars still.

Texture2D<int2> rawFlow : register(t0);
RWTexture2D<float2> motionVectors : register(u0);

cbuffer DensifyConstants : register(b0)
{
    uint gridWidth;
    uint gridHeight;
    uint sourceWidth;
    uint sourceHeight;
    uint outputWidth;
    uint outputHeight;
    uint fitLeft;
    uint fitTop;
    uint fitWidth;
    uint fitHeight;
    uint gridSize;
    uint padding;
};

// The grid vector of one cell, from S10.5 fixed point to source pixels.
float2 gridFlow(int2 cell)
{
    const uint2 clamped = uint2(clamp(cell, int2(0, 0), int2(gridWidth - 1, gridHeight - 1)));

    return float2(rawFlow[clamped]) / 32.0;
}

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= outputWidth || id.y >= outputHeight)
    {
        return;
    }

    const bool inPicture = id.x >= fitLeft && id.y >= fitTop &&
                            id.x < fitLeft + fitWidth && id.y < fitTop + fitHeight;

    if (!inPicture)
    {
        motionVectors[id.xy] = float2(0.0, 0.0);
        return;
    }

    const float sourceX = (float(id.x - fitLeft) + 0.5) * sourceWidth / float(fitWidth);
    const float sourceY = (float(id.y - fitTop) + 0.5) * sourceHeight / float(fitHeight);

    // Each grid vector sits at the center of its cell; blend the four around
    // the pixel, repeating the edge cells past the grid.
    const float2 gridPosition = float2(sourceX, sourceY) / float(gridSize) - 0.5;
    const float2 cellFloor = floor(gridPosition);
    const float2 weight = gridPosition - cellFloor;
    const int2 cell = int2(cellFloor);

    const float2 top = lerp(gridFlow(cell), gridFlow(cell + int2(1, 0)), weight.x);
    const float2 bottom = lerp(gridFlow(cell + int2(0, 1)), gridFlow(cell + int2(1, 1)), weight.x);
    const float2 flow = lerp(top, bottom, weight.y);

    motionVectors[id.xy] = flow * float2(fitWidth, fitHeight) / float2(sourceWidth, sourceHeight);
}
