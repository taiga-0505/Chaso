struct SpriteVSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// C++ の struct Material と同じレイアウトにする
struct Material
{
    float4 color; // 色 (RGBA)
    int lightingMode; // 0:なし, 1:Lambert, 2:Half Lambert
    float3 padding; // アラインメント調整
    float4x4 uvTransform; // UV変換行列
};

// ★ b0 にする！（cbMat_ がバインドされてるレジスタ）
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(SpriteVSOutput input) : SV_TARGET
{
    // UV 変換
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float2 tex = uv.xy;

    float4 texColor = gTexture.Sample(gSampler, tex);

    // αテスト
    if (texColor.a <= 0.5f)
        discard;

    return texColor * gMaterial.color;
}
