#include "../Fullscreen/Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

PixelShaderOutPut main(VertexShaderOutput input) {
    PixelShaderOutPut output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 周囲を0に、中心になるほど明るくなるように計算で調整
    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    // correctだけで計算すると中心の最大値が0.0625で暗すぎるのでScaleで調整。16倍して1にする
    float vignette = correct.x * correct.y * 16.0f;
    // 0.8乗でそれっぽくする
    vignette = saturate(pow(vignette, 0.8f));
    // 係数として乗算
    output.color.rgb *= vignette;

    return output;
}
