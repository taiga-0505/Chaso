#include "Object3d.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// SV_IsFrontFace で表裏を判定し、色分けする
// 表面（Front Face）→ 青系
// 裏面（Back Face）→  赤系
PixelShaderOutput main(VertexShaderOutput input, bool isFrontFace : SV_IsFrontFace)
{
    PixelShaderOutput output;

    // 法線をビュー空間的に使ってグラデーションを付ける（立体感のため）
    float3 N = normalize(input.normal);
    float shade = abs(dot(N, float3(0.0f, 0.0f, -1.0f))) * 0.3f + 0.7f;

    if (isFrontFace)
    {
        // 表面: 青 (Blender準拠)
        output.color = float4(0.22f * shade, 0.46f * shade, 0.82f * shade, 1.0f);
    }
    else
    {
        // 裏面: 赤 (Blender準拠)
        output.color = float4(0.82f * shade, 0.22f * shade, 0.22f * shade, 1.0f);
    }

    output.color *= input.instColor;
    return output;
}
