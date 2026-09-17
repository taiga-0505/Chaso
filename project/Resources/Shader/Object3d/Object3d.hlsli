struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float4 instColor : COLOR0;
    float4 lightSpacePos : TEXCOORD1;
};

// ----------------------------------------------------------------------------
// Shadow Params
// ----------------------------------------------------------------------------
struct ShadowData {
    matrix lightViewProjection;
    float3 lightDirection;
    float bias;
    float4 color;
    int shadowMapEnabled;
    float2 shadowMapTexelSize; // 1テクセルのUVサイズ (1/width, 1/height)
    float pcfRadius;           // PCFのタップ間隔（テクセル単位。0以下なら1タップ＝PCF無効）
};

ConstantBuffer<ShadowData> gShadowParams : register(b6);
Texture2D<float> gShadowMap : register(t4);
SamplerState gShadowSampler : register(s2);              // s2 = Linear Clamp
SamplerComparisonState gShadowCmpSampler : register(s3); // s3 = 深度比較サンプラ (PCF用)

// ----------------------------------------------------------------------------
// PCF (Percentage Closer Filtering)
//   3x3 のタップを比較サンプラで取り、「光が当たっている率」を 0〜1 で返す。
//   比較サンプラ自体が 1 タップあたり 2x2 のバイリニア比較を行うため、
//   3x3 タップと合わせて実質 6x6 相当の滑らかさが得られる。
// ----------------------------------------------------------------------------
float SampleShadowPCF(float2 uv, float compareDepth)
{
    // PCF 無効時（半径0以下）は 1 タップだけ取る
    if (gShadowParams.pcfRadius <= 0.0f)
    {
        return gShadowMap.SampleCmpLevelZero(gShadowCmpSampler, uv, compareDepth);
    }

    float2 offsetStep = gShadowParams.shadowMapTexelSize * gShadowParams.pcfRadius;

    float litSum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 tapUV = uv + float2(x, y) * offsetStep;
            litSum += gShadowMap.SampleCmpLevelZero(gShadowCmpSampler, tapUV, compareDepth);
        }
    }

    return litSum / 9.0f;
}

// ----------------------------------------------------------------------------
// Spot Light Shadow (スポットライトごとの影)
//   1 枚のアトラス (t5) を tilesX × tilesY のタイルに割り、タイル 1 枚が
//   1 灯分の透視投影シャドウマップになる。SpotLight::shadowIndex がタイル番号。
//   Object3d.PS 側でスポットライトの寄与に「光が当たっている率」を掛けることで、
//   壁を挟んだ向こう側へ光が漏れなくなる。
// ----------------------------------------------------------------------------
static const uint MAX_SPOT_SHADOWS = 32;

struct SpotShadowEntry
{
    matrix lightViewProjection; // ライト視点の ViewProjection
    float nearZ;                // ライト視点の near
    float farZ;                 // ライト視点の far（= 到達距離）
    float tanHalfFov;           // 照射半角の tan
    float pad;
    float3 lightPosition;       // ライトのワールド座標（真下向きモードで使う）
    float pad2;
};

cbuffer SpotShadowCB : register(b7)
{
    SpotShadowEntry gSpotShadowEntries[MAX_SPOT_SHADOWS];
    uint gSpotShadowCount;
    uint gSpotShadowTilesX;
    uint gSpotShadowTilesY;
    float gSpotShadowTileSizePx;      // タイル 1 枚の解像度 (px)
    float2 gSpotShadowAtlasTexelSize; // アトラス 1 テクセルの UV サイズ
    float gSpotShadowBias;            // 定数バイアス（ワールド単位の線形距離）
    float gSpotShadowSlopeBias;       // 斜面バイアス係数
    float gSpotShadowPcfRadius;       // PCF タップ間隔（テクセル単位。0以下で1タップ）
    float3 _padSpotShadow;
};

Texture2D<float> gSpotShadowAtlas : register(t5);

/// @brief 指定タイルの深度マップで「光が当たっている率」を 0〜1 で返す（共通処理）
/// @param shadowIndex アトラスのタイル番号（0 以上）
/// @param samplePos   遮蔽を調べる位置（ワールド座標）
/// @param N           受光点の法線（正規化済み）
/// @param L           受光点 → ライトの方向（正規化済み）
float SampleShadowTile(int shadowIndex, float3 samplePos, float3 N, float3 L)
{
    if (shadowIndex < 0 || (uint)shadowIndex >= gSpotShadowCount)
    {
        return 1.0f;
    }

    SpotShadowEntry e = gSpotShadowEntries[shadowIndex];

    float3 worldPos = samplePos;
    float4 lightPos = mul(float4(worldPos, 1.0f), e.lightViewProjection);
    // ライトの背後は照らされない（スポット係数側でも 0 になる）
    if (lightPos.w <= 1e-4f)
    {
        return 0.0f;
    }

    // far より遠い点は深度に変換すると 1 を超え、比較サンプラが必ず「影」を返してしまう。
    // 範囲外は判定不能なので「当たっている」扱いにする（タイル外と同じ方針）
    if (lightPos.w >= e.farZ)
    {
        return 1.0f;
    }

    float2 ndc = lightPos.xy / lightPos.w;
    float2 tileUV = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);

    // 照射範囲（＝シャドウマップの範囲）外は判定不能なので「当たっている」扱い
    if (any(tileUV < 0.0f) || any(tileUV > 1.0f))
    {
        return 1.0f;
    }

    // -----------------
    // バイアス（線形距離で扱う）
    //   透視投影の深度は非線形なので、w（ライトからの距離）に対して
    //   「1 テクセルのワールドサイズ × 斜面係数 + 定数」を引いてから深度へ変換する。
    //   受光面が光に対して斜めなほど 1 テクセルが面上で長くなるため tanθ でスケールする。
    // -----------------
    float ndl = saturate(dot(N, L));
    float tanTheta = sqrt(saturate(1.0f - ndl * ndl)) / max(ndl, 0.05f);
    tanTheta = min(tanTheta, 8.0f);

    float texelWorld = (2.0f * lightPos.w * e.tanHalfFov) / max(gSpotShadowTileSizePx, 1.0f);
    float biasedDist = max(lightPos.w - (gSpotShadowBias + texelWorld * gSpotShadowSlopeBias * (1.0f + tanTheta)), e.nearZ + 1e-4f);

    // 線形距離 → 透視投影の深度値（MakePerspectiveFovMatrix と同じ式）
    float compareDepth = (e.farZ / (e.farZ - e.nearZ)) * (1.0f - e.nearZ / biasedDist);

    // -----------------
    // タイル → アトラス UV
    // -----------------
    uint tilesX = max(gSpotShadowTilesX, 1u);
    uint tilesY = max(gSpotShadowTilesY, 1u);
    float2 tileScale = float2(1.0f / (float)tilesX, 1.0f / (float)tilesY);
    uint tx = (uint)shadowIndex % tilesX;
    uint ty = (uint)shadowIndex / tilesX;
    float2 tileOrigin = float2((float)tx, (float)ty) * tileScale;

    // 隣のタイルへ滲まないよう、タップ位置を「タイル内側 1 テクセル分」にクランプする
    float2 tileMin = tileOrigin + gSpotShadowAtlasTexelSize;
    float2 tileMax = tileOrigin + tileScale - gSpotShadowAtlasTexelSize;
    float2 uv = tileOrigin + tileUV * tileScale;

    // -----------------
    // PCF 3x3
    // -----------------
    if (gSpotShadowPcfRadius <= 0.0f)
    {
        return gSpotShadowAtlas.SampleCmpLevelZero(gShadowCmpSampler, clamp(uv, tileMin, tileMax), compareDepth);
    }

    float2 offsetStep = gSpotShadowAtlasTexelSize * gSpotShadowPcfRadius;
    float litSum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 tapUV = clamp(uv + float2(x, y) * offsetStep, tileMin, tileMax);
            litSum += gSpotShadowAtlas.SampleCmpLevelZero(gShadowCmpSampler, tapUV, compareDepth);
        }
    }
    return litSum / 9.0f;
}

/// @brief スポットライトの影を判定し「光が当たっている率」を 0〜1 で返す
/// @param shadowIndex SpotLight::shadowIndex（0 以上でアトラスのタイル番号）
/// @param worldPos    受光点のワールド座標
/// @param N           受光点の法線（正規化済み）
/// @param L           受光点 → ライトの方向（正規化済み）
float SampleSpotShadow(int shadowIndex, float3 worldPos, float3 N, float3 L)
{
    return SampleShadowTile(shadowIndex, worldPos, N, L);
}

/// @brief 点光源・面光源（全方位に光る光源）の影を判定し「光が当たっている率」を返す
/// @details 全方位ぶんの深度マップ（キューブマップ）を持つ代わりに、
///          「ライト位置から真下を向いた広角の透視投影」1 枚だけで判定する。
///          このゲームの遮蔽物（壁）は垂直なので、ある水平方向が遮られているかは高さに依らない。
///          そこで受光点をその真下（円錐の内側に入るところまで）へずらしてから引く。
///          これでライトより高い壁面や壁の天面も正しく遮蔽される。
///          シャドウマップは裏面を描く方式（second-depth）なので、遮蔽物の内部にある点
///          （壁自身の天面など）は「当たっている」と判定され、黒く潰れない。
/// @param shadowIndex PointLight/AreaLight::shadowIndex（0 以上でアトラスのタイル番号）
/// @param worldPos    受光点のワールド座標
/// @param N           受光点の法線（正規化済み）
/// @param L           受光点 → ライトの方向（正規化済み）
float SampleOmniShadowDown(int shadowIndex, float3 worldPos, float3 N, float3 L)
{
    if (shadowIndex < 0 || (uint)shadowIndex >= gSpotShadowCount)
    {
        return 1.0f;
    }

    SpotShadowEntry e = gSpotShadowEntries[shadowIndex];

    // ライトからの水平距離。この距離が円錐に収まる深さまでサンプル点を下げる
    float2 horiz = worldPos.xz - e.lightPosition.xz;
    float r = length(horiz);
    float minDepth = (r / max(e.tanHalfFov, 1e-3f)) * 1.08f; // 端に寄りすぎないよう少し余裕を持たせる
    float depth = max(e.lightPosition.y - worldPos.y, max(minDepth, e.nearZ + 1e-3f));

    float3 samplePos = float3(worldPos.x, e.lightPosition.y - depth, worldPos.z);
    return SampleShadowTile(shadowIndex, samplePos, N, L);
}
