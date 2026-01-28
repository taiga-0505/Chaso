cbuffer PerFrame : register(b0)
{
    row_major float4x4 gView;
    row_major float4x4 gProj;
};

struct VSIn
{
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VSOut
{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

VSOut main(VSIn v)
{
    VSOut o;
    float4 wp = float4(v.pos, 1.0f);
    float4 vp = mul(wp, gView);
    o.svpos = mul(vp, gProj);
    o.color = v.color;
    return o;
}
