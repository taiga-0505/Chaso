#include "Object3d.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 単色のワイヤーフレーム色（白）
    output.color = float4(0.8f, 0.8f, 0.8f, 1.0f) * input.instColor;
    
    return output;
}
