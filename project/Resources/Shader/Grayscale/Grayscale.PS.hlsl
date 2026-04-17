#include "../Fullscreen/Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

PixelShaderOutPut main(VertexShaderOutput input) {
    PixelShaderOutPut output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    // BT.709 Grayscale
    float32_t value = dot(output.color.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    output.color.rgb = float32_t3(value, value, value);
    return output;
}
