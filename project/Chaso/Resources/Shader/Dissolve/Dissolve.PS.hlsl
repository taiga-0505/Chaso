#include "../Fullscreen/Fullscreen.hlsli"

// t0: シーンテクスチャ
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// t1: ノイズテクスチャ（マスク）
Texture2D<float32_t> gMaskTexture : register(t1);

// b1: Dissolveパラメータ
cbuffer DissolveParams : register(b1) {
    float4 edgeColor;    // Edge発光色 (RGB + alpha)
    float4 baseColor;    // 抜けた部分の背景色 (RGB + alpha)
    float threshold;     // 閾値 (0.0 ~ 1.0)
    float edgeRange;     // Edge検出幅 (default: 0.03)
    float2 _padding;
};

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

PixelShaderOutPut main(VertexShaderOutput input) {
    PixelShaderOutPut output;

    // マスクテクスチャからノイズ値をサンプリング
    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    // マスク値が閾値以下の場合はベースカラーを出力
    if (mask <= threshold) {
        output.color = baseColor;
        return output;
    }

    // シーンテクスチャをサンプリング
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // Edgeっぽさを算出: threshold 〜 threshold+edgeRange の範囲を 0〜1 に補間
    // 1から引くことで、閾値に近いほどEdgeっぽさが高くなる
    float32_t edge = 1.0f - smoothstep(threshold, threshold + edgeRange, mask);

    // Edgeっぽいほど指定した色を加算
    output.color.rgb += edge * edgeColor.rgb;

    return output;
}
