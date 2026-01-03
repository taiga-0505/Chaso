Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer Params : register(b0)
{
    float4 A; // p0.xy p1.xy（ピクセル座標）
    float4 B; // rectMin.xy rectMax.xy（ピクセル座標）
    float4 C; // screenSize.xy thickness feather
    float4 D; // center.xy radius unused
    uint4 U; // shapeType flags unused unused
    float4 Color;
    float4 UVRect; // uvMin.xy uvMax.xy（0..1）
};

static const uint SHAPE_LINE = 0;
static const uint SHAPE_RECT = 1;
static const uint SHAPE_CIRCLE = 2;
static const uint SHAPE_SPRITE = 3;

static const uint FLAG_STROKE = 1; // 1=枠線, 0=塗り
static const uint FLAG_TEX = 2; // 1=テクスチャ使用

float sdBox(float2 p, float2 mn, float2 mx)
{
    float2 c = (mn + mx) * 0.5;
    float2 h = (mx - mn) * 0.5;
    float2 q = abs(p - c) - h;
    return length(max(q, 0)) + min(max(q.x, q.y), 0);
}
float sdCircle(float2 p, float2 c, float r)
{
    return length(p - c) - r;
}
float distSeg(float2 p, float2 a, float2 b)
{
    float2 ab = b - a;
    float ab2 = dot(ab, ab);
    if (ab2 < 1e-6)
        return length(p - a);
    float t = clamp(dot(p - a, ab) / ab2, 0.0, 1.0);
    return length(p - (a + t * ab));
}

float4 main(float4 svPos : SV_POSITION, float2 uvIn : TEXCOORD) : SV_Target
{
    float2 p = svPos.xy;

    uint shape = U.x;
    uint flags = U.y;

    float2 p0 = A.xy;
    float2 p1 = A.zw;

    float2 rectMin = B.xy;
    float2 rectMax = B.zw;

    float2 screen = C.xy;
    float thickness = C.z;
    float feather = C.w;

    float2 center = D.xy;
    float radius = D.z;

    bool stroke = (flags & FLAG_STROKE) != 0;
    bool useTex = (flags & FLAG_TEX) != 0;

  // テクスチャモード（汎用に入れておく）
    if (shape == SHAPE_SPRITE || useTex)
    {
        float2 mn = rectMin;
        float2 mx = rectMax;
        if (p.x < mn.x || p.y < mn.y || p.x > mx.x || p.y > mx.y)
            discard;

        float2 t = (p - mn) / max(mx - mn, 1e-3.xx);
        float2 uv = lerp(UVRect.xy, UVRect.zw, t);
        return gTex.Sample(gSamp, uv) * Color;
    }

    float halfW = thickness * 0.5;

  // まず「塗り」のsigned distance
    float sd = 0.0;
    if (shape == SHAPE_LINE)
    {
        sd = distSeg(p, p0, p1) - halfW; // 線はこの時点で枠線相当
    }
    else if (shape == SHAPE_RECT)
    {
        sd = sdBox(p, rectMin, rectMax);
        if (stroke)
            sd = abs(sd) - halfW;
    }
    else if (shape == SHAPE_CIRCLE)
    {
        sd = sdCircle(p, center, radius);
        if (stroke)
            sd = abs(sd) - halfW;
    }

  // AA（featherが0でも多少は綺麗にしたいなら fwidth 混ぜる）
    float aa = max(feather, fwidth(sd));
    float alpha = 1.0 - smoothstep(0.0, aa, sd);

    if (alpha <= 0.0)
        discard;
    return float4(Color.rgb, Color.a * alpha);
}
