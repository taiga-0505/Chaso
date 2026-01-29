#include "Object3d.hlsli"

struct Material
{
    float4 color; // rgb=tint, a=opacity(不透明度)
    int lightingMode; // 0:なし, 1:Lambert, 2:HalfLambert（ガラスでも使ってOK）
    float shininess; // スペキュラ指数（ガラスは 256～1024 くらいが映える）
    float2 padding; // x=IOR, y=roughness(0..1) として使う
    float4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;
    float radius;
    float decay;
    float2 padding;
};
static const uint MAX_POINT_LIGHTS = 4;
cbuffer PointLightsCB : register(b3)
{
    uint pointCount;
    float3 _padPoint;
    PointLight pointLights[MAX_POINT_LIGHTS];
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float2 padding;
};
static const uint MAX_SPOT_LIGHTS = 4;
cbuffer SpotLightsCB : register(b4)
{
    uint spotCount;
    float3 _padSpot;
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

struct AreaLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 right;
    float halfWidth;
    float3 up;
    float halfHeight;
    float range;
    float decay;
    uint twoSided;
    uint pad;
};
static const uint MAX_AREA_LIGHTS = 4;
cbuffer AreaLightsCB : register(b5)
{
    uint areaCount;
    float3 _padArea;
    AreaLight areaLights[MAX_AREA_LIGHTS];
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// HalfLambert/Lambert共通化
float CalcDiffuseTerm(float NdotL, int lightingMode)
{
    if (lightingMode == 2)
    {
        float h = saturate(NdotL * 0.5f + 0.5f);
        return h * h;
    }
    return saturate(NdotL);
}

// Schlick Fresnel
float FresnelSchlick(float cosTheta, float F0)
{
    float x = 1.0f - saturate(cosTheta);
    float x5 = x * x * x * x * x;
    return F0 + (1.0f - F0) * x5;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV変換 & サンプル
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 tex = gTexture.Sample(gSampler, uv.xy);

    float4 base = gMaterial.color * tex * input.instColor;

    // ガラスは cutout しない（完全透明だけ捨てる）
    if (base.a <= 0.001f)
        discard;

    // ライティング無しなら透明色だけ（※プレマルチ前提）
    if (gMaterial.lightingMode == 0)
    {
        output.color.rgb = base.rgb * base.a;
        output.color.a = base.a;
        return output;
    }

    // ==========
    // 共通ベクトル
    // ==========
    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // 2-sided用：裏面に回ったら法線反転（窓板とかで重要）
    if (dot(N, V) < 0.0f)
        N = -N;

    float NdotV = saturate(dot(N, V));

    // ==========
    // Fresnel
    // ==========
    float ior = (gMaterial.padding.x > 1.0f) ? gMaterial.padding.x : 1.5f; // default 1.5
    float f = (1.0f - ior) / (1.0f + ior);
    float F0 = f * f; // ガラスだとだいたい 0.04 付近
    float fresnel = FresnelSchlick(NdotV, F0);

    float rough = saturate(gMaterial.padding.y); // 0..1

    // shininess未指定なら roughness から適当に決める
    float shininess = (gMaterial.shininess > 0.0f)
        ? gMaterial.shininess
        : lerp(1024.0f, 64.0f, rough * rough);

    // ==========
    // ライト合算（元のObject3Dと同じ考え方）
    // ==========
    float3 diffuseSum = 0.0f;
    float3 specularSum = 0.0f;

    // ---- Directional
    {
        float3 L = normalize(-gDirectionalLight.direction);
        float NdotL = dot(N, L);
        float diffuseTerm = CalcDiffuseTerm(NdotL, gMaterial.lightingMode);

        float3 lightCol = gDirectionalLight.color.rgb * gDirectionalLight.intensity;

        diffuseSum += base.rgb * lightCol * diffuseTerm;

        if (NdotL > 0.0f)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // ---- Point (MAX 4)
    [unroll]
    for (uint i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        if (i >= pointCount)
            break;

        PointLight pl = pointLights[i];
        if (pl.intensity <= 0.0f || pl.radius <= 0.0f)
            continue;

        float3 toL = pl.position - input.worldPosition;
        float dist = length(toL);
        if (dist >= pl.radius || dist <= 1e-5f)
            continue;

        float3 L = toL / dist;

        float t = saturate(1.0f - dist / pl.radius);
        float atten = pow(t, max(pl.decay, 0.0001f));

        float NdotL = dot(N, L);
        float diffuseTerm = CalcDiffuseTerm(NdotL, gMaterial.lightingMode);

        float3 lightCol = pl.color.rgb * pl.intensity * atten;

        diffuseSum += base.rgb * lightCol * diffuseTerm;

        if (NdotL > 0.0f)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // ---- Spot (MAX 4)
    [unroll]
    for (uint i = 0; i < MAX_SPOT_LIGHTS; ++i)
    {
        if (i >= spotCount)
            break;

        SpotLight sl = spotLights[i];
        if (sl.intensity <= 0.0f || sl.distance <= 0.0f)
            continue;

        float3 toL = sl.position - input.worldPosition;
        float dist = length(toL);
        if (dist >= sl.distance || dist <= 1e-5f)
            continue;

        float3 L = toL / dist;

        float t = saturate(1.0f - dist / sl.distance);
        float atten = pow(t, max(sl.decay, 0.0001f));

        float3 dirN = normalize(sl.direction);
        float cosLD = dot(-L, dirN);
        float spot = saturate((cosLD - sl.cosAngle) / max(1e-5f, (1.0f - sl.cosAngle)));

        float3 lightCol = sl.color.rgb * sl.intensity * atten * spot;

        float NdotL = dot(N, L);
        float diffuseTerm = CalcDiffuseTerm(NdotL, gMaterial.lightingMode);

        diffuseSum += base.rgb * lightCol * diffuseTerm;

        if (NdotL > 0.0f)
        {
            float3 H = normalize(L + V);
            float specPow = pow(saturate(dot(N, H)), shininess);
            specularSum += lightCol * specPow;
        }
    }

    // ---- Area（元の擬似4サンプルそのまま）
    const uint aCount = min(areaCount, MAX_AREA_LIGHTS);
    [loop]
    for (uint ai = 0; ai < aCount; ++ai)
    {
        AreaLight al = areaLights[ai];
        if (al.intensity <= 0.0f || al.range <= 0.0f)
            continue;

        float3 R = al.right;
        float3 U = al.up;
        float rLen = length(R);
        float uLen = length(U);
        if (rLen < 1e-5f || uLen < 1e-5f)
            continue;
        R /= rLen;
        U /= uLen;

        float3 Ln = cross(R, U);
        float nLen = length(Ln);
        if (nLen < 1e-5f)
            continue;
        Ln /= nLen;

        float3 c = al.position;
        float hw = al.halfWidth;
        float hh = al.halfHeight;

        float3 corners[4] =
        {
            c + R * hw + U * hh,
            c + R * hw - U * hh,
            c - R * hw + U * hh,
            c - R * hw - U * hh
        };

        float3 lightBase = al.color.rgb * al.intensity;

        [unroll]
        for (int si = 0; si < 4; ++si)
        {
            float3 toL = corners[si] - input.worldPosition;
            float dist = length(toL);
            if (dist <= 1e-5f || dist >= al.range)
                continue;

            float3 L = toL / dist;

            if (al.twoSided == 0)
            {
                if (dot(Ln, -L) <= 0.0f)
                    continue;
            }

            float t = saturate(1.0f - dist / al.range);
            float atten = pow(t, max(al.decay, 0.0001f));

            float NdotL = dot(N, L);
            float diffuseTerm = CalcDiffuseTerm(NdotL, gMaterial.lightingMode);

            float3 lightCol = lightBase * atten * 0.25f;

            diffuseSum += base.rgb * lightCol * diffuseTerm;

            if (NdotL > 0.0f)
            {
                float3 H = normalize(L + V);
                float specPow = pow(saturate(dot(N, H)), shininess);
                specularSum += lightCol * specPow;
            }
        }
    }

    // ==========
    // ガラス合成（重要）
    // ==========
    // 不透明度：素材a + (端で少し増える) + (曇りで増える)
    float opacity = saturate(base.a + fresnel * 0.15f + rough * 0.25f);

    // 反射：フレネルで端が強く、roughで弱く
    float specScale = lerp(1.0f, 0.25f, rough);
    float3 reflection = specularSum * specScale * (0.2f + 2.0f * fresnel);

    // 透過っぽい“にじみ”：すりガラスほど増える（超控えめ）
    float scatter = lerp(0.02f, 0.15f, rough);
    float3 transmissionLit = diffuseSum * scatter;

    // ★プレマルチ出力：rgbは alpha を掛けた“透過色” + 反射（反射はαで薄まらない）
    output.color.rgb = saturate(base.rgb * opacity + transmissionLit * opacity + reflection);
    output.color.a = opacity;

    return output;
}
