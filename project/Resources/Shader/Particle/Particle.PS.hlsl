#include "Particle.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV 変換＆テクスチャサンプル
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float2 transformedUV = uv.xy;
    float4 textureColor = gTexture.Sample(gSampler, transformedUV);
    
    output.color = gMaterial.color * textureColor * input.color;
    
    if(output.color.a == 0.0f)
    {
        discard;
    }
    return output;
}
