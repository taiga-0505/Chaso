#include "../Object3d/Object3d.hlsli"

// ----------------------------------------------------------------------------
// Skydome 用頂点シェーダ
// ----------------------------------------------------------------------------
// Object3D_Single.VS.hlsl と同じ入出力だが、クリップ座標の z を w に置き換えて
// 深度を常に最遠 (z/w = 1) にする（Skybox.VS.hlsl と同じ手法）。
//
// これにより、カメラの Far クリップ距離や天球の半径・カメラ位置に関係なく
// 天球は「無限遠の背景」として描画され、Far より奥にある頂点が
// クリップされて消える問題が起きない。
// PSO 側（object3d_skydome）は深度テスト LESS_EQUAL・深度書き込み OFF で使う。
// ----------------------------------------------------------------------------

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 worldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 clipPos = mul(input.position, gTransformationMatrix.WVP);
    // z = w にして深度を最遠に固定（Far クリップに掛からない）
    output.position = clipPos.xyww;

    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.worldInverseTranspose));
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    output.instColor = float4(1, 1, 1, 1);

    // Object3D.PS と出力レイアウトを揃えるため計算しておく（天球は lightingMode=0 なので未使用）
    output.lightSpacePos = mul(float4(output.worldPosition, 1.0f), gShadowParams.lightViewProjection);

    return output;
}
