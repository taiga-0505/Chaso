#include "ECS/ScriptableEntity.h"
#include "ECS/ScriptRegistry.h"
#include "ECS/LightComponent.h"
#include "ECS/TransformComponent.h"
#include "ECS/WaterComponent.h"
#include "ECS/CameraComponent.h"
#include "Application/Game/Framework/WaterCameraFx.h"
#include "Scene.h"

#if RC_ENABLE_IMGUI
#include "imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>

/// @class CaveLightZoneScript
/// @brief 屋外 ⇔ 洞窟を 1 面の中で行き来するとき、カメラの位置で太陽光と環境光を切り替える
/// @details
///   平行光源には影が無いので、洞窟の中に入っても天井に遮られず外と同じ明るさで照らされてしまう。
///   そこで、メインカメラの z が fromZ → toZ と進むあいだに、平行光源の強さ・色・環境光と
///   水面の映り込み（WaterComponent::environmentCoeff）を a → b へなめらかに補間する。
///     Stage2（洞窟に入っていく）: a = 屋外, b = 洞窟内
///     Stage5（洞窟から抜ける）  : a = 洞窟内, b = 屋外
///
///   置き場所: Directional Light のエンティティ（DirectionalLightComponent を持つもの）。
///   再生中だけ書き換える。編集中に書き換えると、その値がシーンの保存で JSON に焼き付くため。
///
///   JSON (scriptDataList):
///     "fromZ" / "toZ" : 補間を始める／終える z（fromZ > toZ でもよい）
///     "a" / "b"       : { "intensity", "color"[4], "ambientColor"[3], "ambientIntensity", "waterEnvironment",
///                          "waterShallow"[4], "waterDeep"[4] }（水面の色は a / b 両方にあるときだけ補間）
class CaveLightZoneScript : public ScriptableEntity {
public:
  struct Look {
    float intensity = 1.0f;
    RC::Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    RC::Vector3 ambientColor = {1.0f, 1.0f, 1.0f};
    float ambientIntensity = 0.0f;
    float waterEnvironment = 0.6f;
    /// @brief 水面の色（浅い所・深い所）。両方の Look で hasWaterColor が true のときだけ補間する
    bool hasWaterColor = false;
    RC::Vector4 waterShallow = {0.10f, 0.50f, 0.60f, 0.85f};
    RC::Vector4 waterDeep = {0.02f, 0.10f, 0.25f, 0.95f};

    nlohmann::json ToJson() const {
      nlohmann::json j = {{"intensity", intensity},
              {"color", {color.x, color.y, color.z, color.w}},
              {"ambientColor", {ambientColor.x, ambientColor.y, ambientColor.z}},
              {"ambientIntensity", ambientIntensity},
              {"waterEnvironment", waterEnvironment}};
      if (hasWaterColor) {
        j["waterShallow"] = {waterShallow.x, waterShallow.y, waterShallow.z, waterShallow.w};
        j["waterDeep"] = {waterDeep.x, waterDeep.y, waterDeep.z, waterDeep.w};
      }
      return j;
    }
    void FromJson(const nlohmann::json &j) {
      if (!j.is_object()) return;
      if (j.contains("intensity")) intensity = j["intensity"].get<float>();
      if (j.contains("color") && j["color"].size() >= 4) {
        const auto &c = j["color"];
        color = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
      }
      if (j.contains("ambientColor") && j["ambientColor"].size() >= 3) {
        const auto &c = j["ambientColor"];
        ambientColor = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>()};
      }
      if (j.contains("ambientIntensity")) ambientIntensity = j["ambientIntensity"].get<float>();
      if (j.contains("waterEnvironment")) waterEnvironment = j["waterEnvironment"].get<float>();
      auto readV4 = [&](const char *key, RC::Vector4 &out) {
        if (!j.contains(key) || !j[key].is_array() || j[key].size() < 4) return false;
        const auto &c = j[key];
        out = {c[0].get<float>(), c[1].get<float>(), c[2].get<float>(), c[3].get<float>()};
        return true;
      };
      const bool s = readV4("waterShallow", waterShallow);
      const bool d = readV4("waterDeep", waterDeep);
      hasWaterColor = s && d;
    }
  };

  float fromZ = 0.0f;
  float toZ = 30.0f;
  Look a;
  Look b{0.3f, {0.7f, 0.72f, 0.8f, 1.0f}, {0.5f, 0.5f, 0.58f}, 0.25f, 0.12f};

  nlohmann::json Serialize() override {
    return {{"fromZ", fromZ}, {"toZ", toZ}, {"a", a.ToJson()}, {"b", b.ToJson()}};
  }
  void Deserialize(const nlohmann::json &j) override {
    if (j.contains("fromZ")) fromZ = j["fromZ"].get<float>();
    if (j.contains("toZ")) toZ = j["toZ"].get<float>();
    if (j.contains("a")) a.FromJson(j["a"]);
    if (j.contains("b")) b.FromJson(j["b"]);
  }

  void OnImGui() override {
#if RC_ENABLE_IMGUI
    ImGui::Text("blend = %.2f", blend_);
    ImGui::DragFloat("From Z", &fromZ, 0.5f);
    ImGui::DragFloat("To Z", &toZ, 0.5f);
    auto edit = [](const char *label, Look &l) {
      if (ImGui::TreeNode(label)) {
        ImGui::DragFloat("Intensity", &l.intensity, 0.01f, 0.0f, 4.0f);
        ImGui::ColorEdit4("Color", &l.color.x);
        ImGui::ColorEdit3("Ambient Color", &l.ambientColor.x);
        ImGui::DragFloat("Ambient Intensity", &l.ambientIntensity, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Water Environment", &l.waterEnvironment, 0.01f, 0.0f, 1.0f);
        ImGui::TreePop();
      }
    };
    edit("A (from)", a);
    edit("B (to)", b);
#endif
  }

protected:
  void OnCreate() override { Apply(); }
  void OnUpdate(float) override { Apply(); }

private:
  static float Smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  }
  static float Lerp(float x, float y, float t) { return x + (y - x) * t; }

  void Apply() {
    SceneContext *ctx = GetSceneContext();
    if (!ctx || !ctx->isPlaying()) return;
    Scene *scene = GetScene();
    auto cam = WaterCameraFx::FindMainCamera(scene);
    if (!cam) return;
    auto *camTr = cam->GetComponent<TransformComponent>();
    if (!camTr) return;

    const float span = toZ - fromZ;
    const float t = (std::fabs(span) < 1e-3f) ? (camTr->position.z >= toZ ? 1.0f : 0.0f)
                                              : Smooth01((camTr->position.z - fromZ) / span);
    blend_ = t;

    if (auto *light = GetComponent<DirectionalLightComponent>()) {
      light->intensity = Lerp(a.intensity, b.intensity, t);
      light->color = {Lerp(a.color.x, b.color.x, t), Lerp(a.color.y, b.color.y, t),
                      Lerp(a.color.z, b.color.z, t), Lerp(a.color.w, b.color.w, t)};
      light->ambientColor = {Lerp(a.ambientColor.x, b.ambientColor.x, t),
                             Lerp(a.ambientColor.y, b.ambientColor.y, t),
                             Lerp(a.ambientColor.z, b.ambientColor.z, t)};
      light->ambientIntensity = Lerp(a.ambientIntensity, b.ambientIntensity, t);
    }
    if (scene) {
      for (const auto &e : scene->GetEntities()) {
        if (!e) continue;
        if (auto *water = e->GetComponent<WaterComponent>()) {
          water->environmentCoeff = Lerp(a.waterEnvironment, b.waterEnvironment, t);
          if (a.hasWaterColor && b.hasWaterColor) {
            auto lerp4 = [&](const RC::Vector4 &x, const RC::Vector4 &y) {
              return RC::Vector4{Lerp(x.x, y.x, t), Lerp(x.y, y.y, t), Lerp(x.z, y.z, t), Lerp(x.w, y.w, t)};
            };
            water->shallowColor = lerp4(a.waterShallow, b.waterShallow);
            water->deepColor = lerp4(a.waterDeep, b.waterDeep);
          }
          break;
        }
      }
    }
  }

  float blend_ = 0.0f;
};

REGISTER_SCRIPT(CaveLightZoneScript)
