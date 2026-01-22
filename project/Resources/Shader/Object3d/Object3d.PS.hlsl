#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int lightingMode; // 0:なし, 1:Lambert, 2:Half Lambert（※3/4が来てもLambert扱い）
    float shininess; // Blinn-Phongの指数（0以下なら鏡面なし）
    float2 padding; // アラインメント調整
    float4x4 uvTransform; // UV変換
};

ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction; // 正規化済み想定
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Camera> gCamera : register(b2);

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

    // 2値抜き（必要なら閾値を調整）
    if (textureColor.a <= 0.5f /* || textureColor.a == 0.0f || output.color.a == 0.0f*/)
    {
        discard;
    }

    // ベース色（材質×テクスチャ）
    float4 base = gMaterial.color * textureColor;

    // ライティングなし
    if (gMaterial.lightingMode == 0)
    {
        output.color = base;
        return output;
    }

    // =========================
    // Lambert / Half-Lambert + Blinn-Phong（固定）
    // =========================

    float3 N = normalize(input.normal);

    // L は「面 → 光」方向に統一（Directionalは -direction でOK）
    float3 L = normalize(-gDirectionalLight.direction);

    // V は「面 → 視点」
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    float NdotL = dot(N, L);

    // 拡散（Lambert or Half-Lambert）
    float diffuseTerm = 0.0f;
    if (gMaterial.lightingMode == 2)
    {
        // Half-Lambert（暗部を持ち上げ）
        float h = saturate(NdotL * 0.5f + 0.5f);
        diffuseTerm = h * h;
    }
    else
    {
        diffuseTerm = saturate(NdotL);
    }

    float3 lightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

    // Diffuse
    float3 diffuse = base.rgb * lightCol * diffuseTerm;

    // Specular（Blinn-Phong固定）
    float3 specular = 0.0f;
    if (gMaterial.shininess > 0.0f && NdotL > 0.0f) // 裏面にハイライト出さない
    {
        float3 H = normalize(L + V);
        float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);
        specular = lightCol * specPow; // 白ハイライト
    }

    output.color.rgb = diffuse + specular;
    output.color.a = base.a;

    return output;
}
