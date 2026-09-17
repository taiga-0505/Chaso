// ============================================================================
// EmitElectric.CS.hlsl
// ----------------------------------------------------------------------------
// 電撃 (ParticleType::Electric) 用の射出 Compute Shader。
// 「モデルの周りをビリビリと電流がまとう」演出を想定し、
// エミッタ位置を中心とした殻（丸みのある箱 / 球。ElectricShell.hlsli 参照）の
// 表面から少し外側にパーティクルを発生させ、表面に沿う方向（接線方向）へ初速を与える。
//
// Default (EmitParticle.CS) との違い:
//   - 発生位置は体積の中ではなく「殻の表面 + マージン」。モデルの中に埋まって隠れない
//   - 初速は殻の接線方向（法線に直交）。UpdateElectric がこの上を這わせる
//   - pad0 にパーティクル固有の乱数 (0〜1) を書き込み、
//     浮かせる距離 / UpdateElectric のジッター位相 / ElectricParticle.PS の稲妻シードに使う
//
// u0 : gParticles      — パーティクルデータ
// u1 : gFreeListIndex  — FreeList の現在のインデックス
// u2 : gFreeList       — 空きパーティクルインデックスの配列
// b0 : gPerFrame       — エミッタパラメータ
//
// Dispatch(1, 1, 1) で emitCount スレッド（最大1024）実行。
// ============================================================================

struct Particle
{
    float3 translate;
    float pad0;      ///< 電撃ではパーティクル固有の乱数 (0〜1) として使う
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

cbuffer PerFrame : register(b0)
{
    // 基本パラメータ (16 bytes)
    float gDeltaTime;
    uint gMaxParticles;
    float gMinLifeTime;
    float gMaxLifeTime;

    // スケール (16 bytes)
    float gMinScale;
    float gMaxScale;
    float gGravity;
    uint gEmitterShape;

    // 速度 (16 bytes)
    float3 gBaseVelocity;
    float gVelocityVariance;

    // 形状パラメータ (16 bytes)
    // ※ 他タイプの gShapePad.xy に当たる 2 つを、電撃では殻の丸みとマージンに使う
    float gShapeRadius;
    float gConeAngle;
    float gShellRoundness;
    float gShellMargin;

    // 開始色 (16 bytes)
    float4 gStartColor;

    // 終了色 (16 bytes)
    float4 gEndColor;

    // エミッタ位置 (16 bytes)
    float3 gEmitterPosition;
    uint gEmitCount;

    // Box形状サイズ (16 bytes)
    // ※ 他タイプの gShapeBoxPad に当たる 1 つを、電撃では殻の Y 軸回転に使う
    float3 gShapeBoxSize;
    float gShellYaw;
};

RWStructuredBuffer<Particle> gParticles     : register(u0);
RWStructuredBuffer<int>      gFreeListIndex : register(u1);
RWStructuredBuffer<uint>     gFreeList      : register(u2);

#include "ElectricShell.hlsli"

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= gEmitCount)
    {
        return;
    }

    // FreeList のインデックスを1つ前に設定し、現在のインデックスを取得
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (0 <= freeListIndex && freeListIndex < (int)gMaxParticles)
    {
        uint particleIndex = gFreeList[freeListIndex];
        uint seed = particleIndex * 1973u + DTid.x * 6547u + 9277u;

        Particle p = (Particle)0;

        // --- 固有の乱数（浮かせる距離 / ジッター位相 / PS の稲妻シード）---
        p.pad0 = Hash(seed + 8u);

        // --- 発生位置: 殻の表面 + マージン ---
        // 内側の箱の中の一様な点から少しだけランダムな方向へずらし、殻の表面へ射影する。
        //   球（内側の箱が一点）→ 方向が一様なので球面上で一様
        //   角のある箱（丸み 0）→ 箱の中の点が最も近い面へ押し出される → 面の上にほぼ一様
        float3 h = ShellHalfExtents();
        float r = ShellCornerRadius(h);
        float3 hi = max(h - r, 0.0f);
        float3 q0 = float3(HashSigned(seed), HashSigned(seed + 1u), HashSigned(seed + 2u)) * hi;
        float3 dir = RandomDirection(seed + 3u);

        float3 normal;
        float3 surface = ProjectToShell(q0 + dir * (r + 0.001f), h, r, normal);
        float3 local = surface + normal * ShellLift(p.pad0);
        p.translate = gEmitterPosition + ShellToWorld(local);

        // --- 初速: 接線方向 ---
        // 法線と乱数ベクトルの外積で接線を作る。乱数が法線と平行に近い場合は
        // 決め打ちの軸で作り直す（normal がその軸と平行なことはない）
        float3 tangent = cross(normal, RandomDirection(seed + 5u));
        if (dot(tangent, tangent) < 1e-6f)
        {
            float3 helper = (abs(normal.x) < 0.9f) ? float3(1.0f, 0.0f, 0.0f)
                                                   : float3(0.0f, 1.0f, 0.0f);
            tangent = cross(normal, helper);
        }
        tangent = normalize(tangent);

        // 速さは Default と同じ意味（1 フレームあたりの移動量）
        float speed = length(gBaseVelocity) + HashSigned(seed + 4u) * gVelocityVariance;
        p.velocity = ShellToWorld(tangent) * max(speed, 0.0f);

        // --- スケール ---
        float s = gMinScale + Hash(seed + 6u) * (gMaxScale - gMinScale);
        p.scale = float3(s, s, s);

        // --- 寿命 ---
        // 電撃は短命が前提（0.1 秒前後）。長くすると火花ではなく「光る玉」になる
        p.lifeTime = gMinLifeTime + Hash(seed + 7u) * (gMaxLifeTime - gMinLifeTime);
        p.currentTime = 0.0f;

        // --- 色: 開始色（以降 UpdateElectric が endColor へ遷移させる）---
        p.color = gStartColor;

        gParticles[particleIndex] = p;
    }
    else
    {
        // 空きがないので、減らした分を戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
