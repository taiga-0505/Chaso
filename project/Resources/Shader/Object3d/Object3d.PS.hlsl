#include "Object3d.hlsli"

struct Material
{
    float4 color; // 色 (RGBA)
    int lightingMode; // 0:なし, 1:Lambert, 2:Half Lambert
    float shininess;
    float2 padding; // アラインメント調整
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color; // ライトの色 (RGBA)
    float3 direction; // ライトの方向ベクトル (正規化されたベクトル)
    float intensity; // ライトの強度
};

struct Camera
{
    float3 worldPosition; // カメラのワールド座標位置
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
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
    
    if (textureColor.a <= 0.5f/* || textureColor.a == 0.0f || output.color.a == 0.0f*/)
    {
        discard;
    }
    
    if (gMaterial.lightingMode == 0)
    {
        // ライティングなし
        output.color = gMaterial.color * textureColor;
    }
    else if (gMaterial.lightingMode == 3)
    {
        // ===== Phong =====
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);

        // 拡散
        float diffuseCos = max(dot(N, L), 0.0f);

        // 反射ベクトル（光の入射方向は gDirectionalLight.direction でOK）
        float3 R = reflect(gDirectionalLight.direction, N);
        float specularPow = pow(saturate(dot(R, V)), gMaterial.shininess);

        float3 lightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
        float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

        float3 diffuse = baseColor * lightCol * diffuseCos;
        float3 specular = lightCol * specularPow; // 白っぽいハイライト

        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
    // ===== Lambert / Half Lambert =====
        float3 N = normalize(input.normal);
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = dot(N, L);

        float lighting = 1.0f;
        if (gMaterial.lightingMode == 1)
        {
        // Lambert
            lighting = max(NdotL, 0.0f);
        }
        else if (gMaterial.lightingMode == 2)
        {
        // Half Lambert
            float halfLambert = NdotL * 0.5f + 0.5f;
            lighting = halfLambert * halfLambert;
        }

        float3 lightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;
        float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

        output.color.rgb = baseColor * lightCol * lighting;
        output.color.a = gMaterial.color.a * textureColor.a;
    }

    return output;
}
