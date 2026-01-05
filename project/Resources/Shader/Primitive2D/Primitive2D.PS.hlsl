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
static const uint SHAPE_TRIANGLE = 4;

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

float cross2(float2 a, float2 b)
{
    return a.x * b.y - a.y * b.x;
}

// Signed distance to a triangle (negative inside)
// based on Inigo Quilez's sdTriangle
float sdTriangle(float2 p, float2 p0, float2 p1, float2 p2)
{
    float2 e0 = p1 - p0;
    float2 e1 = p2 - p1;
    float2 e2 = p0 - p2;

    float2 v0 = p - p0;
    float2 v1 = p - p1;
    float2 v2 = p - p2;

    float d0 = max(dot(e0, e0), 1e-6);
    float d1 = max(dot(e1, e1), 1e-6);
    float d2 = max(dot(e2, e2), 1e-6);

    // 最短距離（大きさ）
    float2 pq0 = v0 - e0 * clamp(dot(v0, e0) / d0, 0.0, 1.0);
    float2 pq1 = v1 - e1 * clamp(dot(v1, e1) / d1, 0.0, 1.0);
    float2 pq2 = v2 - e2 * clamp(dot(v2, e2) / d2, 0.0, 1.0);

    float dist2 = dot(pq0, pq0);
    dist2 = min(dist2, dot(pq1, pq1));
    dist2 = min(dist2, dot(pq2, pq2));

    // 内外判定（符号）: 3辺の半平面テストで決める
    float s = sign(cross2(e0, p2 - p0)); // 三角形の向き（CW/CCW吸収）
    float w0 = s * cross2(e0, p - p0);
    float w1 = s * cross2(e1, p - p1);
    float w2 = s * cross2(e2, p - p2);

    float inside = step(0.0, min(w0, min(w1, w2))); // 1=内側, 0=外側

    float d = sqrt(dist2);
    return lerp(d, -d, inside); // 内側をマイナスに
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

    float2 p2 = B.xy; // triangle 用

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
    else if (shape == SHAPE_TRIANGLE)
    {
        // A.xy=p0, A.zw=p1, B.xy=p2
        float2 t0 = p0;
        float2 t1 = p1;
        float2 t2 = p2;

        // ざっくりのBBで早めに捨てる（フルスクリーン描画なので効く）
        float pad = halfW + max(feather, 1.0) + 1.0;
        float2 bbMin = min(t0, min(t1, t2)) - pad;
        float2 bbMax = max(t0, max(t1, t2)) + pad;
        if (p.x < bbMin.x || p.y < bbMin.y || p.x > bbMax.x || p.y > bbMax.y)
            discard;

        sd = sdTriangle(p, t0, t1, t2);
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
