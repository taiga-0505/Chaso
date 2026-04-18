#include "Skybox.hlsli"

struct Material
{
    float4 color;
    int lightingMode;
    float shininess;
    float padding[2];
    float4x4 uvTransform;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    // cubemap を方向ベクトルでサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
    return output;
}
