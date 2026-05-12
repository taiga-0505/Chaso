#pragma once

/// @file RC.h
/// @brief エンジンの主要な機能を一括でインクルードするためのマスターヘッダー

#include "EngineConfig.h"

// --- 描画・レンダリング ---
#include "RenderCommon.h"

// --- オーディオ ---
#include "Audio/Sound.h"

// --- カメラ ---
#include "Camera/CameraController.h"
#include "CameraMath.h"

// --- 数学 ---
#include "Math/MathTypes.h"
#include "Math/MathUtils.h"

// --- ライト ---
#include "Light/Directional/DirectionalLightSource.h"
#include "Light/Spot/SpotLightSource.h"
#include "Light/Point/PointLightSource.h"
#include "Light/Area/AreaLightSource.h"

// --- パーティクル ---
#include "Particle.h"
#include "Fire/FireParticle.h"
#include "Rain/RainParticle.h"
#include "Snow/SnowParticle.h"
#include "Circle/CircleParticle.h"
#include "Explosion/ExplosionParticle.h"
#include "Laser/LaserParticle.h"
#include "ImpactSpark/ImpactSparkParticle.h"
#include "wind/WindParticle.h"
