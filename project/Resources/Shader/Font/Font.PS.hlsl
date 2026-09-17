// Font.PS.hlsl
// R8_UNORM のグリフアトラス（カバレッジ）をアルファとして頂点カラーを出力する

struct FontVSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

// C++ 側 SpriteMaterial と同じレイアウト（FontManager は白・identity 固定）
struct Material
{
    float4 color;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float> gAtlas : register(t0);
SamplerState gSampler : register(s0); // Linear

float4 main(FontVSOutput input) : SV_TARGET
{
    float coverage = gAtlas.Sample(gSampler, input.texcoord);

    float4 outColor = input.color * gMaterial.color;
    outColor.a *= coverage;

    // 完全に透明なピクセルは捨てる（Sprite の 0.5 αテストは文字の縁を欠くので使わない）
    if (outColor.a <= 0.001f)
        discard;

    return outColor;
}
