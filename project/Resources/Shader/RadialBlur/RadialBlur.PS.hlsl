#include "../Fullscreen/Fullscreen.hlsli"

// PostEffectConstants (b0)
// param0: center.x (float)
// param1: center.y (float)
// param2: blurWidth (float)
// param3: numSamples (int)
struct PostEffectConstants {
    float2 center;
    float blurWidth;
    int numSamples;
};

ConstantBuffer<PostEffectConstants> gParams : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSamplerClamp : register(s2);

struct PS_OUTPUT {
    float4 color : SV_TARGET;
};

PS_OUTPUT main(VertexShaderOutput input) {
    // 中心から現在のuvに対しての方向を計算
    float2 direction = input.texcoord - gParams.center;
    float3 outputColor = float3(0.0f, 0.0f, 0.0f);
    
    // サンプリング点数を制限（最低1、最大100程度にクランプするのが安全だが、
    // ここではエンジンのパラメータを信頼する）
    int numSamples = max(1, gParams.numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        // 現在のuvから放射状にサンプリング点を進めながらサンプリングしていく
        float2 texcoord = input.texcoord + direction * gParams.blurWidth * (float)i;
        outputColor.rgb += gTexture.Sample(gSamplerClamp, texcoord).rgb;
    }
    
    // 平均化する
    outputColor.rgb /= (float)numSamples;
    
    PS_OUTPUT output;
    output.color = float4(outputColor, 1.0f);
    return output;
}
