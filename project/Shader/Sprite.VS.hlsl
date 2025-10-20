struct VSIn
{
    float3 pos : POSITION; // 頂点座標（ローカル）
    float2 uv : TEXCOORD; // 0-1
};

struct VSOut
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD0;
};

// カメラ共通（b0）: ViewProj
cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj;
};

// オブジェクト個別（b1）: World, BaseColor, UVTransform
cbuffer ObjectCB : register(b1)
{
    float4x4 gWorld;
    float4 gBaseColor; // RGB:乗算色, A:全体アルファ
    float3x3 gUVTransform; // (u,v,1) を掛ける 2D 変換
};

VSOut main(VSIn i)
{
    VSOut o;
    float4 worldPos = mul(float4(i.pos, 1.0f), gWorld);
    o.svpos = mul(worldPos, gViewProj);

    float3 uvh = mul(float3(i.uv, 1.0f), gUVTransform);
    o.uv = uvh.xy;
    return o;
}
