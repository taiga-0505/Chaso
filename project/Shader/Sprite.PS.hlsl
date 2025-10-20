struct VSOut
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer ObjectCB : register(b1)
{
    float4x4 gWorld;
    float4 gBaseColor;
    float3x3 gUVTransform;
};

Texture2D gTex0 : register(t0);
SamplerState gSamp : register(s0);

float4 main(VSOut i) : SV_Target
{
    float4 tex = gTex0.Sample(gSamp, i.uv);
    return tex * gBaseColor; // 完全アンリット
    // 必要ならアルファ閾値:
    // clip(tex.a * gBaseColor.a - 0.001f);
}
