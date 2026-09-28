// Font.VS.hlsl
// スクリーン座標（ピクセル）の頂点を正射影で変換するだけの頂点シェーダ

struct FontVSInput
{
    float4 position : POSITION0; // ピクセル座標 (x, y, 0, 1)
    float2 texcoord : TEXCOORD0; // アトラス UV
    float4 color    : COLOR0;    // 頂点カラー
};

struct FontVSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

// C++ 側 TransformationMatrix と同じレイアウト
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

FontVSOutput main(FontVSInput input)
{
    FontVSOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
