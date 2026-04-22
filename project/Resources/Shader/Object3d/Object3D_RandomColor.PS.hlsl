#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int lightingMode;
    float shininess;
    float environmentCoefficient;
    float padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ハッシュ関数: Wang hash
uint WangHash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

// HSV → RGB 変換
float3 HsvToRgb(float hue, float saturation, float value)
{
    float h6 = hue * 6.0f;
    float c = value * saturation;
    float x = c * (1.0f - abs(fmod(h6, 2.0f) - 1.0f));
    float m = value - c;

    float3 rgb;
    if (h6 < 1.0f)      rgb = float3(c, x, 0.0f);
    else if (h6 < 2.0f) rgb = float3(x, c, 0.0f);
    else if (h6 < 3.0f) rgb = float3(0.0f, c, x);
    else if (h6 < 4.0f) rgb = float3(0.0f, x, c);
    else if (h6 < 5.0f) rgb = float3(x, 0.0f, c);
    else                 rgb = float3(c, 0.0f, x);

    return rgb + m;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 定数バッファの Material.color のみをシードに使用
    // ※ 補間される値 (instColor 等) に asuint() を使うと
    //    ピクセルごとにビットパターンが変わりノイズになるため使用しない
    uint seed = asuint(gMaterial.color.r) ^
                (asuint(gMaterial.color.g) * 2654435761u) ^
                (asuint(gMaterial.color.b) * 374761393u) ^
                (asuint(gMaterial.color.a) * 668265263u);

    seed = WangHash(seed);

    // ハッシュから鮮やかなHSVカラーを生成
    float hue = frac(float(seed) / 4294967296.0f);
    float3 baseColor = HsvToRgb(hue, 0.65f, 0.92f);

    // 法線ベースの軽いシェーディング（立体感）
    float3 N = normalize(input.normal);
    float shade = abs(dot(N, normalize(float3(0.3f, -1.0f, 0.5f)))) * 0.35f + 0.65f;

    output.color = float4(baseColor * shade, 1.0f);
    output.color *= input.instColor;

    return output;
}
