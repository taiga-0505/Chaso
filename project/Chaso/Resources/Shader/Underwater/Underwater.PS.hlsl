struct PixelShaderInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderOutPut {
    float4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

// b1: Underwater Parameters
cbuffer UnderwaterParams : register(b1) {
    float4x4 projectionInverse; // プロジェクション逆行列
    float4 tintColor;     // 水中での色味 (例: 0.2, 0.5, 1.0, 1.0)
    float4 fogColor;      // フォグの色
    float time;           // 経過時間
    float distortionForce;// 歪みの強さ
    float fogStart;       // フォグの開始距離
    float fogEnd;         // フォグの終了距離
    float lerpFactor;     // 水上・水中のブレンド率 (0.0:水上, 1.0:完全な水中)
    float3 _padding;
};

PixelShaderOutPut main(PixelShaderInput input) {
    PixelShaderOutPut output;

    // UV座標の歪み（Distortion）計算
    // 水の揺らぎをlerpFactorに応じて強める
    // 高周波 1 本の sin だと輪郭がギザギザの「縞」になって見えるので、
    // 低周波の波を 2 本ずらして重ね、ゆっくり大きくうねる屈折にする。
    // 画面端では歪みを 0 に落として、clamp による端の伸びを出さない。
    float2 uv = input.texcoord;
    float currentDistortion = distortionForce * lerpFactor;

    float2 edge = 1.0f - abs(uv * 2.0f - 1.0f);           // 端で 0、中央で 1
    float edgeFade = smoothstep(0.0f, 0.15f, min(edge.x, edge.y));

    float wx = sin(uv.y * 5.2f + time * 0.9f) * 0.65f
             + sin(uv.y * 9.1f - uv.x * 3.3f + time * 1.4f) * 0.35f;
    float wy = cos(uv.x * 4.6f + time * 0.8f) * 0.65f
             + cos(uv.x * 8.3f + uv.y * 2.7f + time * 1.1f) * 0.35f;

    // アスペクト比を揃える（横方向は縦の 9/16 で同じ画面上の長さになる）
    float2 offset = float2(wx * 0.5625f, wy) * currentDistortion * edgeFade;

    uv = clamp(uv + offset, 0.0f, 1.0f);

    // 元のシーンの色と深度をサンプリング
    float4 baseColor = gTexture.Sample(gSampler, uv);
    float depthVal = gDepthTexture.Sample(gSampler, uv);
    
    // NDC空間からビュー空間のZ（距離）を計算
    float4 ndc = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depthVal, 1.0f);
    float4 viewPos = mul(ndc, projectionInverse);
    viewPos /= viewPos.w;
    float distance = viewPos.z;
    
    // フォグ係数の計算
    float fogFactor = clamp((distance - fogStart) / (fogEnd - fogStart), 0.0f, 1.0f);
    
    // 背景(Skybox等: distanceが非常に大きい)の場合はフォグを強くかけるなど調整可能
    // 今回は単純に距離に応じたフォグ
    float4 foggedColor = lerp(baseColor, fogColor, fogFactor);
    
    // さらに水中の青みを乗算
    float4 underwaterColor = foggedColor * tintColor;
    
    // lerpFactorで最終的に「水上」と「水中」をブレンド
    output.color = lerp(baseColor, underwaterColor, lerpFactor);
    output.color.a = 1.0f;
    
    return output;
}
