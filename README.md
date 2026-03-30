# Chaso Engine

DirectX 12ベースの自作ゲームエンジンです。
エンジン層 (`ChasoEngine`) とアプリケーション層 (`ChasoApp`) に分離されており、描画・入力・ウィンドウ管理・各種リソース管理の基盤を提供します。

[![DebugBuild](https://github.com/reachaso/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/reachaso/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/reachaso/CG2/actions/workflows/ReleaseBuild.yml)

## 概要 (Overview)

本プロジェクトは、DirectX 12を用いた低レイヤーのグラフィックスプログラミングの学習および、高性能なゲーム制作を目的として開発されています。
エンジン機能は静的ライブラリ (`ChasoEngine`) として分離されており、描画パスの最適化・リソース管理・モダンなC++20の機能を活用したコアアーキテクチャを備えています。
ゲームロジックはアプリケーション層 (`ChasoApp`) に実装されています。

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

## プロジェクト構成 (Project Structure)

本ソリューション (`chaso.sln`) は以下の2プロジェクトで構成されています。

| プロジェクト | 種別 | 役割 |
| :--- | :--- | :--- |
| **ChasoEngine** | 静的ライブラリ (.lib) | 描画・入力・ウィンドウ等のエンジンコア |
| **ChasoApp** | 実行ファイル (.exe) | ゲームロジック・シーン管理 |

## ディレクトリ構成 (Folder Structure)

### Engine (`project/Engine/`)

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Dx12/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Dx12/)** | デバイス、スワップチェーン、パイプライン等のコアラッパー |
| **[Graphics/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Graphics/)** | モデル、スプライト、ライト等の描画オブジェクトの実装 |
| **[Render/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Render/)** | レンダリングフローおよびパイプライン状態の統合管理 |
| **[Particle/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Particle/)** | パーティクル生成・更新・計算ロジック |
| **[Camera/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Camera/)** | カメラの管理と変換行列の計算 |
| **[Window/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Window/)** | Win32ウィンドウ管理、メッセージループ処理 |
| **[Input/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Input/)** | 入力デバイスの状態取得と管理 |
| **[Common/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/Common/)** | 数学ライブラリ、ログ出力、基本構造体 |
| **[ImGuiManager/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Engine/ImGuiManager/)** | ImGuiの初期化・描画フローとの統合 |

### Application (`project/Application/`)

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Framework/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Application/Framework/)** | アプリケーション基盤 (`App`) とコンフィグ管理 |
| **[Game/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Application/Game/)** | ゲームロジック（Player・Scene・MapChip・Goal・Coin 等） |

### その他

| ディレクトリ | 役割 |
| :--- | :--- |
| **[Externals/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Externals/)** | 外部ライブラリ (Assimp / DirectXTex / ImGui / curl / nlohmann) |
| **[Resources/](file:///c:/Users/rea/source/repos/2025/CG2/CG2/project/Resources/)** | テクスチャ、モデル、シェーダ等のリソースファイル |

## ビルド方法 (Build)

1. `project/chaso.sln` を Visual Studio 2022 で開きます。
2. 構成を `Debug`, `Release`, または `Development` に設定します。
3. プラットフォームを `x64` に設定します。
4. `Ctrl + Shift + B` でソリューションをビルドします。
   - `ChasoEngine` が先にビルドされ、`ChasoApp` がそれをリンクします。

## 使用ライブラリ (Libraries)

- [DirectX 12](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Assimp](https://github.com/assimp/assimp)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [ImGui](https://github.com/ocornut/imgui)
- Media Foundation (Sound/Movie playback)

---
> [!IMPORTANT]
> 本エンジンは教育目的であり、実用的なパフォーマンスとメンテナンス性を重視して設計されています。
