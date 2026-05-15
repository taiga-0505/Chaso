#include "../Fullscreen/Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);

SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

struct Material {
    float4x4 projectionInverse;
    float4 outlineColor;
    float outlineWeight;
    float outlineThickness;
    int outlineMode;
    float padding;
};
ConstantBuffer<Material> gMaterial : register(b1);

static const float kPrewittHorizontalKernel[3][3] = {
    { -1.0f, 0.0f, 1.0f },
    { -1.0f, 0.0f, 1.0f },
    { -1.0f, 0.0f, 1.0f }
};

static const float kPrewittVerticalKernel[3][3] = {
    { -1.0f, -1.0f, -1.0f },
    {  0.0f,  0.0f,  0.0f },
    {  1.0f,  1.0f,  1.0f }
};

float4 main(VertexShaderOutput input) : SV_TARGET {
    uint width, height;
    gDepthTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(1.0f / width, 1.0f / height);

    float2 difference = float2(0.0f, 0.0f);
    float centerZ = 0.0f;
    float minZ = 100000.0f;
    float maxZ = -100000.0f;

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            float2 offset = float2(x - 1, y - 1) * uvStepSize * gMaterial.outlineThickness;
            float2 sampleUv = input.texcoord + offset;

            float ndcDepth = gDepthTexture.SampleLevel(gSamplerPoint, sampleUv, 0);
            float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
            float viewZ = viewSpace.z / viewSpace.w;

            if (x == 1 && y == 1) centerZ = viewZ;
            minZ = min(minZ, viewZ);
            maxZ = max(maxZ, viewZ);

            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPrewittVerticalKernel[x][y];
        }
    }

    float weight = length(difference);
    weight = saturate(weight * gMaterial.outlineWeight);

    if (gMaterial.outlineMode == 1) { // Outside
        if (abs(centerZ - minZ) < abs(centerZ - maxZ)) {
            weight = 0.0f;
        }
    } else if (gMaterial.outlineMode == 2) { // Inside
        if (abs(centerZ - maxZ) < abs(centerZ - minZ)) {
            weight = 0.0f;
        }
    }

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    
    return lerp(color, gMaterial.outlineColor, weight);
}
