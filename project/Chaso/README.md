# ChasoEngine

DirectX 12 製の自作ゲームエンジン。ゲームリポジトリからは `project/Chaso/` に **git subtree** として取り込んで使う。

```
ゲームリポジトリ/
  project/
    chaso.sln, main.cpp, AppConfig.json      ← ゲーム側（Template からコピー）
    Application/ChasoApp.vcxproj             ← ゲーム側
    Application/Game/Scripts/                ← ゲーム側（スクリプト）
    Resources/                               ← 実行時に使う唯一の Resources（ゲーム + エンジン由来のコピー）
    Chaso/                                   ← ★ このリポジトリ（subtree）
      Engine/       エンジン本体（ChasoEngine.vcxproj）
      Externals/    imgui / assimp / DirectXTex / nlohmann / curl / httplib
      Framework/    App（アプリのライフサイクル）, AppConfig
      Editor/       EditorManager, CaptureMode（ImGui エディタ）
      Game/         Game, Scene, SceneManager, DataDrivenScene, Fade, Framework/*
      Resources/    エンジン用リソースの原本（ビルド前に project/Resources へ同期される）
      Template/     新しいゲームを作るときに project/ へコピーするひな形（_root/ はリポジトリ直下へ）
      scripts/      同期用スクリプト
```

## リソースの扱い

実行ファイルが見るのは従来どおり **`project/Resources/` ひとつだけ**。配布時もこのフォルダを exe と一緒に置けばよい。

エンジンが必要とするリソース（Shader / icons / fonts / noise / Particle / Template / uvChecker / white1x1）の
原本は ChasoEngine 側の `Chaso/Resources/` にあり、**ビルド前に `scripts/sync-resources.ps1` が自動で同期**する
（`ChasoApp.vcxproj` の PreBuildEvent）:

1. `Chaso/Resources` → `Resources`: 新しい・無いファイルをコピー
2. `Resources` → `Chaso/Resources`: エンジン側に既にあるファイルだけ、更新されていれば書き戻す（新しい方が勝つ）
3. `Resources/.gitignore` を自動生成し、エンジン由来のコピーはゲームリポジトリで追跡しない

つまり **エンジンのシェーダーやフォントは `project/Resources/` で直接編集してよい**。次のビルドで
`Chaso/Resources` に書き戻され、`engine-push.ps1` で ChasoEngine に反映される。
新しいファイルをエンジンに追加するときだけ `Chaso/Resources/` に置く（`Resources/` に置くとゲーム固有扱い）。
削除は同期されないので、両方から消すこと。

保険として `Chaso::ResolvePath()`（`Engine/Common/ResourcePath.h`）が「`Resources/` に無ければ `Chaso/Resources/`」も
探すので、同期前でも起動はできる。

## ゲームごとに変える設定（`project/AppConfig.json`）

```json
{
  "width": 1280, "height": 720, "fullscreen": false,
  "title":     { "Debug": "ChasoEngine", "Development": "〇〇_CG4", "Release": "〇〇_ゲーム名" },
  "bootScene": { "Debug": "Title",       "Development": "Stage1",  "Release": "Title" }
}
```

`title` がウィンドウタイトル、`bootScene` が起動時に読むシーン名（`Resources/Scenes/<name>.json`）。
どちらも文字列 1 つ（全構成共通）でも、`Debug` / `Development` / `Release` の構成別オブジェクトでも書ける
（無い構成は `Release` → 既定値の順で補う）。エディタからの保存（`SaveAppConfig`）はウィンドウ設定だけ上書きし、
これらのキーは壊さない。

## 日常の使い方（ゲームリポジトリ側で実行）

| やりたいこと | コマンド |
|---|---|
| 新しいゲームを作る | `.\project\Chaso\scripts\new-game.ps1 -Name MyGame`（`D:\production\MyGame` に作る。引数なしなら名前を聞かれる。場所を変えるなら `-Dest`） |
| 作ったゲームを GitHub に公開（初回 push） | `.\project\Chaso\scripts\publish-github.ps1`（gh があれば自動作成、無ければ URL を聞く） |
| リソースを手動で同期する（通常はビルド前に自動） | `.\project\Chaso\scripts\sync-resources.ps1` |
| ゲーム中にエンジンを直した → ChasoEngine へ反映 | `.\project\Chaso\scripts\engine-push.ps1` |
| ChasoEngine の更新を取り込む | `.\project\Chaso\scripts\engine-pull.ps1` |

- `engine-push` は `project/Chaso/` 配下に触れたコミットだけを切り出して送る。ゲームのコードは送られない。
- 別のゲームで先にエンジンが更新されていて push が拒否されたら、`engine-pull` → 競合解決 → `engine-push`。
- エンジンだけ直したいときは、どれかのゲームリポジトリの `project/Chaso/` で直して push すればよい。
  ChasoEngine を単体で clone しても Visual Studio では開けない（sln はゲーム側にある）。

VS Code の Project Actions（`.project-actions.json`）にも同じボタンがある:
「エンジン反映」「エンジン取込」「リソース同期」「新規ゲーム」「GitHub公開」。新規ゲームにもこのボタン設定と `scripts/build/` が付いてくる。

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
