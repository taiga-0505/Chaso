#include "Skybox.hlsli"

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
    // WVP 変換後、z を w にコピーして深度を最遠 (z/w = 1) にする
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    // 頂点位置をそのまま方向ベクトルとして出力（cubemapサンプリング用）
    output.texcoord = input.position.xyz;
    return output;
}
