// FogOverlay.hlsl

cbuffer FogCB : register(b0)
{
    float gTime; // 秒
    float gIntensity; // 0..1
    float gScale; // 例: 2.0〜8.0
    float gSpeed; // 例: 0.02〜0.15

    float2 gWind; // 例: (0.05, 0.02)
    float gFeather; // 霧の柔らかさ 例: 0.2
    float gBottomBias; // 下側濃くする 例: 0.4
    
    float4 gColor;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VS(uint vid : SV_VertexID)
{
    // Fullscreen triangle (頂点バッファ不要)
    float2 p;
    p.x = (vid == 2) ? 3.0 : -1.0;
    p.y = (vid == 1) ? 3.0 : -1.0;

    VSOut o;
    o.pos = float4(p, 0.0, 1.0);
    // -1..3 を 0..1 に正規化（少しはみ出すけどOK）
    o.uv = p * 0.5 + 0.5;
    return o;
}

// 軽量ハッシュ/ノイズ
float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = hash12(i);
    float b = hash12(i + float2(1, 0));
    float c = hash12(i + float2(0, 1));
    float d = hash12(i + float2(1, 1));
    float2 u = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p)
{
    float v = 0.0;
    float a = 0.5;
    // 4オクターブくらいが軽くてちょうどいい
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        v += a * noise(p);
        p *= 2.02;
        a *= 0.5;
    }
    return v;
}

float4 PS(VSOut i) : SV_TARGET
{
    float2 uv = saturate(i.uv);

    // 風で流す
    float2 p = uv * gScale + gWind * (gTime * gSpeed);

    // 霧っぽい“もや”＝fbm + ちょい歪み（簡易ドメインワープ）
    float n1 = fbm(p);
    float2 warp = float2(fbm(p + 7.1), fbm(p + 13.7)) - 0.5;
    float n2 = fbm(p + warp * 0.8);

    float mist = lerp(n1, n2, 0.65);

    // ふわっとさせる（閾値をぼかす）
    float alpha = smoothstep(0.45 - gFeather, 0.45 + gFeather, mist);

    // 下側ほど濃く（寒い霧が溜まる感じ）
    float bottom = smoothstep(0.0, 1.0, uv.y); // 上が1
    float bottomFactor = lerp(1.0 + gBottomBias, 1.0, bottom);
    alpha *= bottomFactor;
    
    alpha = saturate(alpha * gIntensity * gColor.a);
    
    return float4(gColor.rgb, alpha);
}
