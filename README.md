# master
[![DebugBuild](https://github.com/reachaso/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/reachaso/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/ReleaseBuild.yml)

# GE3 branch
[![GE3DebugBuild](https://github.com/reachaso/CG2/actions/workflows/GE3DebugBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/GE3DebugBuild.yml)
[![GE3ReleaseBuild](https://github.com/reachaso/CG2/actions/workflows/GE3ReleaseBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/GE3ReleaseBuild.yml)

# Chaso Engine

DirectX 12ベースの自作ゲームエンジンです。
アプリケーション層から独立しており、描画、入力、ウィンドウ管理、各種リソース管理の基盤を提供します。

[![GE3DebugBuild](https://github.com/reachaso/CG2/actions/workflows/GE3DebugBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/GE3DebugBuild.yml)
[![GE3ReleaseBuild](https://github.com/reachaso/CG2/actions/workflows/GE3ReleaseBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/GE3ReleaseBuild.yml)

## 概要 (Overview)

本エンジンは、DirectX 12を用いた低レイヤーのグラフィックスプログラミングの学習および、高性能なゲーム制作を目的として開発されています。
描画パスの最適化、リソース管理、モダンなC++20の機能を活用したコアアーキテクチャを備えています。

## 主な機能 (Features)

### レンダリング (Rendering)
- **3Dグラフィックス**:
  - [Assimp](https://github.com/assimp/assimp) による多種多様な3Dモデル（.obj, .fbx等）の読み込み。
  - カスタム頂点バッファ・インデックスバッファによる高速な描画。
  - テクスチャマッピングおよびUVアニメーション。
- **2Dグラフィックス**:
  - スプライト描画、2Dプリミティブ描画。
- **パーティクルシステム**:
  - インスタンス描画による大量のパーティクル制御（爆発、花火、雨、雪など）。
- **ライティング**:
  - 平行光源 (Directional Light)
  - 点光源 (Point Light)
  - スポットライト (Spot Light)
  - 面光源 (Area Light)

### システム (System)
- **ウィンドウ管理**: Win32 APIによる堅牢なウィンドウ制御とメッセージループ。
- **入力管理**: DirectInput / XInputによるマルチコントローラー、マウス、キーボード入力。
- **FPS制御**: デルタタイム計算および60FPS固定機能。
- **シェーダ**: HLSL (Shader Model 6.0+) 対応。

### ツール (Tools)
- **デバッグGUI**: [ImGui](https://github.com/ocornut/imgui) を統合し、実行時にリアルタイムでパラメータ調整が可能。

## 動作環境 (Requirements)

- **OS**: Windows 10 / 11 (x64)
- **DirectX**: DirectX 12 (Feature Level 12.0以上)
- **IDE**: Visual Studio 2022 (v143セット)
- **言語**: C++20

## ディレクトリ構成 (Folder Structure)

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Dx12/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Dx12/)** | デバイス、スワップチェーン、パイプライン等のコアラッパー |
| **[Graphics/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Graphics/)** | モデル、スプライト、ライト等の描画オブジェクトの実装 |
| **[Render/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Render/)** | レンダリングフローおよびパイプライン状態の統合管理 |
| **[Particle/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Particle/)** | パーティクル生成・更新・計算ロジック |
| **[Window/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Window/)** | Win32ウィンドウ管理、メッセージループ処理 |
| **[Input/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Input/)** | 入力デバイスの状態取得と管理 |
| **[Common/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/Common/)** | 数学ライブラリ、ログ出力、基本構造体 |
| **[ImGuiManager/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/engine/ImGuiManager/)** | ImGuiの初期化・描画フローとの統合 |

## ビルド方法 (Build)

1. `project/CG2.sln` を Visual Studio 2022 で開きます。
2. 構成を `Debug`, `Release`, または `Development` に設定します。
3. プラットフォームを `x64` に設定します。
4. `Ctrl + Shift + B` でソリューションをビルドします。

## 使用ライブラリ (Libraries)

- [DirectX 12](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Assimp](https://github.com/assimp/assimp)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [ImGui](https://github.com/ocornut/imgui)
- Media Foundation (Sound/Movie playback)

---
> [!IMPORTANT]
> 本エンジンは教育目的であり、実用的なパフォーマンスとメンテナンス性を重視して設計されています。
