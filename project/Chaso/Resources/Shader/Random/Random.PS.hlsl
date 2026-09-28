struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// b1: RandomNoise Parameters
cbuffer RandomNoiseParams : register(b1) {
    float4 noiseColor;
    float time;
    float intensity;
    float2 _padding;
};

// 乱数生成アルゴリズム
float32_t rand2dTo1d(float2 value) {
    return frac(sin(dot(value, float2(12.9898, 78.233))) * 43758.5453);
}

PixelShaderOutPut main(PixelShaderInput input) {
    PixelShaderOutPut output;

    // 経過時間(time)を掛けてSeed値を変化させる
    float32_t random = rand2dTo1d(input.texcoord * time);
    
    // 元のシーンの色をサンプリング
    float4 baseColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 乱数の値を入力画像に乗算
    float4 noisyColor = baseColor * float4(random, random, random, 1.0f) * noiseColor;
    
    // 強度(intensity)に応じて元の色とノイズのかかった色をブレンド
    output.color = lerp(baseColor, noisyColor, intensity);
    
    return output;
}
