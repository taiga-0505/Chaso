# ChasoEngine

DirectX 12 製の自作ゲームエンジン。ゲームリポジトリからは `project/Chaso/` に **git subtree** として取り込んで使う。

```
ゲームリポジトリ/
  project/
    chaso.sln, main.cpp, AppConfig.json      ← ゲーム側（Template からコピー）
    Application/ChasoApp.vcxproj             ← ゲーム側
    Application/Game/Scripts/                ← ゲーム側（スクリプト）
    Resources/                               ← ゲーム側（モデル・音・シーン JSON など）
    Chaso/                                   ← ★ このリポジトリ（subtree）
      Engine/       エンジン本体（ChasoEngine.vcxproj）
      Externals/    imgui / assimp / DirectXTex / nlohmann / curl / httplib
      Framework/    App（アプリのライフサイクル）, AppConfig
      Editor/       EditorManager, CaptureMode（ImGui エディタ）
      Game/         Game, Scene, SceneManager, DataDrivenScene, Fade, Framework/*
      Resources/    エンジンが必要とするリソース（Shader, icons, fonts, noise, Particle, Template）
      Template/     新しいゲームを作るときに project/ へコピーするひな形
      scripts/      同期用スクリプト
```

## リソースの解決ルール

コード中のパスは従来どおり `"Resources/..."` のまま。`Chaso::ResolvePath()`（`Engine/Common/ResourcePath.h`）が

1. ゲーム側 `project/Resources/...`
2. エンジン側 `project/Chaso/Resources/...`

の順に探す。ゲーム側に同名ファイルを置けばエンジン側を上書きできる。
テクスチャ・モデル・音・シェーダー・フォント・JSON の各ローダーに適用済み。

## ゲームごとに変える設定（`project/AppConfig.json`）

```json
{ "width": 1280, "height": 720, "fullscreen": false,
  "title": "ゲーム名", "bootScene": "Title" }
```

`title` がウィンドウタイトル、`bootScene` が起動時に読むシーン名（`Resources/Scenes/<name>.json`）。

## 日常の使い方（ゲームリポジトリ側で実行）

| やりたいこと | コマンド |
|---|---|
| 新しいゲームを作る | `.\project\Chaso\scripts\new-game.ps1 -Name MyGame -Dest D:\` |
| ゲーム中にエンジンを直した → ChasoEngine へ反映 | `.\project\Chaso\scripts\engine-push.ps1` |
| ChasoEngine の更新を取り込む | `.\project\Chaso\scripts\engine-pull.ps1` |

- `engine-push` は `project/Chaso/` 配下に触れたコミットだけを切り出して送る。ゲームのコードは送られない。
- 別のゲームで先にエンジンが更新されていて push が拒否されたら、`engine-pull` → 競合解決 → `engine-push`。
- エンジンだけ直したいときは、どれかのゲームリポジトリの `project/Chaso/` で直して push すればよい。
  ChasoEngine を単体で clone しても Visual Studio では開けない（sln はゲーム側にある）。

## 手動で同じことをする場合

```powershell
# 反映
git subtree push --prefix=project/Chaso https://github.com/taiga-0505/ChasoEngine.git main
# 取り込み
git subtree pull --prefix=project/Chaso https://github.com/taiga-0505/ChasoEngine.git main --squash
# 新規ゲームへ追加
git subtree add  --prefix=project/Chaso https://github.com/taiga-0505/ChasoEngine.git main --squash
```

## 注意

- `project/Chaso/` の中にゲーム固有のファイルを置かない（push でエンジン側に混ざる）。
- `Game/Framework/` の `UnderwaterLook`, `WaterCameraFx`, `InkScreenFx`, `StageProgress` は
  水天の射手由来だが `SceneManager` / `DataDrivenScene` が参照しているためエンジン側に置いている。
  汎用化するときはここから切り離す。
- `Externals/assimp/lib/*.lib` は追跡対象。それ以外の `.lib` と `DirectXTex/Shaders/Compiled/` は無視される。
