// NVIDIA's optical flow is a coarse grid of S10.5 vectors over the source
// picture; DLSS Frame Generation wants one current-to-previous vector per
// back-buffer pixel, in pixels at that resolution. The source is drawn
// letterboxed into the back buffer, so the shader maps through the fit
// rectangle, scales the vectors with it and leaves the bars still.

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
    const uint cellX = min(uint(sourceX) / gridSize, gridWidth - 1);
    const uint cellY = min(uint(sourceY) / gridSize, gridHeight - 1);
    const float2 flow = float2(rawFlow[uint2(cellX, cellY)]) / 32.0;

    motionVectors[id.xy] = flow * float2(fitWidth, fitHeight) / float2(sourceWidth, sourceHeight);
}
