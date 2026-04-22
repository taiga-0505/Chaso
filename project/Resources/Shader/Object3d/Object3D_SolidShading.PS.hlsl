#include "Object3d.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// シンプルなHalf-Lambert（テクスチャなし、形状確認用）
// 固定の平行光源で凹凸を強調する
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 N = normalize(input.normal);

    // 固定ライト方向（斜め上から）
    float3 L = normalize(float3(0.3f, -0.8f, 0.6f));
    float NdotL = dot(N, -L);

    // Half-Lambert: 暗部を柔らかく持ち上げ、形状の凹凸を把握しやすく
    float halfLambert = NdotL * 0.5f + 0.5f;
    halfLambert = halfLambert * halfLambert;

    // 環境光を低めに設定（形状の凹凸を強調するため）
    float ambient = 0.15f;
    float lighting = halfLambert * 0.85f + ambient;

    // ベースカラー: ニュートラルなグレー
    float3 baseColor = float3(0.78f, 0.78f, 0.80f);

    output.color = float4(baseColor * lighting, 1.0f);
    output.color *= input.instColor;

    return output;
}
