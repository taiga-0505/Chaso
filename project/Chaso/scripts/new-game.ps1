<#
.SYNOPSIS
  ChasoEngine を使った新しいゲームリポジトリを作る。

.DESCRIPTION
  1. 空の git リポジトリを作成
  2. ChasoEngine を project/Chaso に subtree add（--squash）
  3. project/Chaso/Template の中身（main.cpp, sln, vcxproj, AppConfig.json など）を project/ にコピー
  4. 初回コミット

  出来上がった構成:
    <Dest>/<Name>/
      .gitignore, .project-actions.json, .vscode/, scripts/build/   ← Template/_root から
      project/
        chaso.sln, main.cpp, AppConfig.json, GameSettings.json
        Application/ChasoApp.vcxproj, Application/Game/Scripts/
        Resources/Scenes/Title.json      ← ゲームのリソース
        Chaso/                           ← エンジン（subtree）

.EXAMPLE
  .\project\Chaso\scripts\new-game.ps1 -Name MyNewGame -Dest D:\
  .\project\Chaso\scripts\new-game.ps1            # 名前を聞かれる。作成先は今のリポジトリと同じ階層
#>
param(
  [string]$Name,
  [string]$Dest,
  [string]$Remote = "https://github.com/taiga-0505/ChasoEngine.git",
  [string]$Branch = "main"
)
$ErrorActionPreference = "Stop"

if (-not $Name) {
  $Name = Read-Host "新しいゲームの名前（リポジトリ名）"
  if (-not $Name) { throw "名前が空です" }
}
if (-not $Dest) {
  # 既定: 今いる git リポジトリの親フォルダ（例: D:\Chaso で実行 → D:\）
  $here = git rev-parse --show-toplevel 2>$null
  $Dest = if ($here) { Split-Path -Parent $here } else { (Get-Location).Path }
}

$target = Join-Path $Dest $Name
if (Test-Path $target) { throw "$target は既に存在します" }

New-Item -ItemType Directory -Path $target | Out-Null
Set-Location $target

git init -b master | Out-Null
# subtree add には 1 つ以上のコミットが必要なので、先に README だけコミットする
"# $Name`n`nChasoEngine ベースのゲーム。エンジン本体は project/Chaso（ChasoEngine の subtree）。`n" |
  Set-Content -Encoding UTF8 README.md
git add README.md
git commit -q -m "init: $Name"

Write-Host "==> ChasoEngine を project/Chaso に取り込み" -ForegroundColor Cyan
git subtree add --prefix=project/Chaso $Remote $Branch --squash -m "engine: add ChasoEngine $Branch"

Write-Host "==> テンプレートを展開" -ForegroundColor Cyan
$tpl = Join-Path $target "project\Chaso\Template"
# _root/ 以外 → project/ 、 _root/ の中身 → リポジトリ直下
Get-ChildItem -Path $tpl -Force | Where-Object { $_.Name -ne "_root" } | ForEach-Object {
  Copy-Item -Path $_.FullName -Destination (Join-Path $target "project") -Recurse -Force
}
$tplRoot = Join-Path $tpl "_root"
if (Test-Path $tplRoot) {
  Get-ChildItem -Path $tplRoot -Force | ForEach-Object {
    Copy-Item -Path $_.FullName -Destination $target -Recurse -Force
  }
}
# タイトルをゲーム名に（構成別）
$cfg = Join-Path $target "project\AppConfig.json"
if (Test-Path $cfg) {
  $j = Get-Content $cfg -Raw -Encoding UTF8 | ConvertFrom-Json
  $j.title = [ordered]@{ Debug = "$Name (Debug)"; Development = "$Name (Development)"; Release = $Name }
  $j | ConvertTo-Json -Depth 5 | Set-Content $cfg -Encoding UTF8
}

# エンジンのリソースを project/Resources に展開（ビルド前にも自動で走る）
& (Join-Path $target "project\Chaso\scripts\sync-resources.ps1")

git add -A
git commit -q -m "game: bootstrap from ChasoEngine template"

Write-Host ""
Write-Host "完了: $target" -ForegroundColor Green
Write-Host "  Visual Studio で project\chaso.sln を開いてビルドしてください。"
Write-Host "  エンジンを直したら: .\project\Chaso\scripts\engine-push.ps1"
Write-Host "  エンジンの更新を取り込む: .\project\Chaso\scripts\engine-pull.ps1"
