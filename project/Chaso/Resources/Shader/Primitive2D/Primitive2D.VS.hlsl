struct VSIn
{
    float4 pos : POSITION; // clip座標
    float2 uv : TEXCOORD;
};
struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};
VSOut main(VSIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv = i.uv;
    return o;
}
