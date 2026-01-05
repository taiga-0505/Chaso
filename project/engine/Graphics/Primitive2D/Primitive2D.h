#pragma once
#include <cstdint>
#include <d3d12.h>

#include "Math/Math.h"
#include "function/function.h" // CreateBufferResource
#include "struct.h"

class Primitive2D {
public:
  Primitive2D() = default;
  ~Primitive2D();

  void Initialize(ID3D12Device *device, float screenW, float screenH);
  void SetScreenSize(float w, float h);
  void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE srv) { srv_ = srv; }

  // ---- shape setters ----
  void SetLine(const RC::Vector2 &p0, const RC::Vector2 &p1, float thickness,
               const RC::Vector4 &color, float feather = 1.0f);

  void SetRect(const RC::Vector2 &mn, const RC::Vector2 &mx,
               kFillMode fillMode, float thickness,
               const RC::Vector4 &color, float feather = 1.0f);

  void SetCircle(const RC::Vector2 &c, float r, kFillMode fillMode,
                 float thickness, const RC::Vector4 &color,
                 float feather = 1.0f);

  void SetTriangle(const RC::Vector2 &p0, const RC::Vector2 &p1,
                   const RC::Vector2 &p2, kFillMode fillMode,
                   float thickness, const RC::Vector4 &color,
                   float feather = 1.0f);

  // もしPS側を rect基準UVに直したらこれも使える
  void SetSpriteRect(const RC::Vector2 &mn, const RC::Vector2 &mx,
                     const RC::Vector4 &color,
                     const RC::Vector2 &uvMin = {0, 0},
                     const RC::Vector2 &uvMax = {1, 1});

  void BeginFrame(); 

  void Draw(ID3D12GraphicsCommandList *cmdList);

private:
  void Release();

public:
  struct Vertex {
    RC::Vector4 pos; // clip
    RC::Vector2 uv;
  };

private:
  struct VB {
    ID3D12Resource *res = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view{};
  };

  template <class T> struct CB {
    ID3D12Resource *res = nullptr;
    T *map = nullptr;
  };

  struct UInt4 {
    uint32_t x, y, z, w;
  };

  // HLSL Params と同レイアウトにする
  struct alignas(16) Params {
    RC::Vector4 A;
    RC::Vector4 B;
    RC::Vector4 C;
    RC::Vector4 D;
    UInt4 U;
    RC::Vector4 Color;
    RC::Vector4 UVRect;
  };

  struct alignas(16) DummyVS {
    RC::Matrix4x4 World;
    RC::Matrix4x4 WVP;
  };

  enum : uint32_t {
    SHAPE_LINE = 0,
    SHAPE_RECT = 1,
    SHAPE_CIRCLE = 2,
    SHAPE_SPRITE = 3,
    SHAPE_TRIANGLE = 4
  };
  enum : uint32_t { FLAG_STROKE = 1, FLAG_TEX = 2 };

  static constexpr uint32_t kMaxDrawPerFrame = 256;
  static constexpr uint32_t Align256(uint32_t v) { return (v + 255u) & ~255u; }

  // CPU側に保持する最新Params
  Params paramsCPU_{};

  // リング化したCB
  ID3D12Resource *cbParamsRes_ = nullptr;
  uint8_t *cbParamsMap_ = nullptr;
  uint32_t cbStride_ = 0;
  uint32_t cbCursor_ = 0;

  ID3D12Device *device_ = nullptr;
  float screenW_ = 0.0f;
  float screenH_ = 0.0f;

  VB vb_{};
  CB<Params> cbParams_{};
  CB<DummyVS> cbDummy_{};

  D3D12_GPU_DESCRIPTOR_HANDLE srv_{};
  bool visible_ = true;
};
