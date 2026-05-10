# SETUP: Mac (Apple Silicon) での UE5 開発環境構築

本書は Apple Silicon Mac で UE5 + C++ プロジェクトを動かすまでの手順。  
推奨: UE 5.5 (Apple Silicon ネイティブ、Lumen/Nanite が Metal で安定動作する世代)。

---

## 0. 前提

- Apple Silicon Mac (M1 以降)
- macOS Sonoma 14.0 以降
- 空き容量 80GB 以上 (UE5 Editor 30GB + Xcode 15GB + DDC/Intermediate で増えるため)
- Epic Games アカウント

---

## 1. Xcode と Command Line Tools

UE5 の C++ ビルドには Xcode が必要。

```bash
# 1-1. App Store から Xcode 15 以降をインストール (GUI)
#      https://apps.apple.com/app/xcode/id497799835

# 1-2. 起動して License に同意
sudo xcodebuild -license accept

# 1-3. Command Line Tools
xcode-select --install

# 1-4. アクティブな developer dir を Xcode 本体に向ける
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

# 1-5. 確認
xcodebuild -version
clang --version
```

---

## 2. Homebrew と Git LFS

UE5 のバイナリアセットは Git LFS で管理する (本リポジトリの `.gitattributes` 参照)。

```bash
# 2-1. Homebrew (未導入なら)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2-2. Git LFS
brew install git-lfs

# 2-3. リポジトリで LFS を有効化 (この repo で1回だけ)
cd ~/ghq/github.com/ochanuco/callsign-exfil
git lfs install
```

---

## 3. Epic Games Launcher と UE5

```bash
# 3-1. Epic Games Launcher を DL してインストール (GUI)
#      https://store.epicgames.com/ja/download
#
# 3-2. Launcher を起動して Epic アカウントでログイン
#
# 3-3. 左ペイン "Unreal Engine" → "Library" → "+" で UE 5.5.x をインストール
#      - Options で以下のみ有効化を推奨 (容量節約):
#        - Editor symbols for debugging  (任意)
#        - macOS  (必須)
#        - iOS    (将来モバイル展開しないなら外す)
#      - Engine Source / Templates は ON のまま
```

インストール後の確認:

```bash
ls /Users/Shared/Epic\ Games/UE_5.5/Engine/Build/BatchFiles/Mac/
# GenerateProjectFiles.sh などが見えれば OK
```

---

## 4. UE5 プロジェクトを `game/` に作成

設計判断:
- プロジェクトは `<repo>/game/` 配下に作る (リポジトリ直下のドキュメントと分離)
- C++ + Blueprint プロジェクト
- Third Person テンプレート (肩越しTPSの土台として最短)
- プロジェクト名: `CallsignExfil` (ファイル/クラスのプレフィックスになる)

手順:

1. Epic Games Launcher → "Unreal Engine" → "Library" → 5.5.x の "Launch"
2. New Project ダイアログで:
   - Category: **Games**
   - Template: **Third Person**
   - Project Defaults:
     - **C++** を選択 (Blueprint ではなく)
     - Quality: **Maximum**
     - Raytracing: **Off** (Mac の Metal では実質効かないため)
     - Starter Content: **Off** (後で必要になったら追加)
3. Location を本リポジトリの `game/` に指定
4. Project Name に `CallsignExfil` を入力
5. Create

完了後の構成:

```
callsign-exfil/
├── ADR.md
├── PLAN.md
├── SETUP.md
├── README.md
├── LICENSE
├── .gitignore
├── .gitattributes
└── game/
    └── CallsignExfil/
        ├── CallsignExfil.uproject
        ├── Source/
        ├── Content/
        ├── Config/
        └── ...
```

---

## 5. 初回ビルドと起動確認

UE Editor が初回プロジェクト作成時に自動で C++ をビルドし、Editor が立ち上がる。  
立ち上がりを確認したら一旦閉じてから、CLI から再ビルドできることも確認しておく。

```bash
cd ~/ghq/github.com/ochanuco/callsign-exfil/game/CallsignExfil

# Xcode プロジェクト生成
/Users/Shared/Epic\ Games/UE_5.5/Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh \
  -project="$PWD/CallsignExfil.uproject" -game -engine

# Xcode workspace を開く (任意)
open CallsignExfil.xcworkspace
```

Editor を CLI から起動したい場合:

```bash
/Users/Shared/Epic\ Games/UE_5.5/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor \
  "$PWD/CallsignExfil.uproject"
```

---

## 6. VSCode での編集 (任意、推奨)

UE5 は VSCode 用の compile_commands.json と launch.json を生成できる。  
Editor の "Editor Preferences → Source Code → Source Code Editor" を `Visual Studio Code` に変更してから、メニューの "Tools → Refresh Visual Studio Code Project" を実行。

```bash
# C/C++ 拡張は最低限入れておく
code --install-extension ms-vscode.cpptools
code --install-extension epicgames.unrealengine  # 公式 Unreal 拡張 (Marketplace)
```

---

## 7. リポジトリ初回コミット (UE プロジェクト作成後)

`game/CallsignExfil/` を作成した直後に大量の Binaries/ Intermediate/ Saved/ が生まれているが、それらは `.gitignore` で除外済み。確認:

```bash
git status
git check-ignore -v game/CallsignExfil/Intermediate/  # 除外されていれば OK
```

LFS で追跡されるべきファイル (.uasset / .umap など) があるか確認:

```bash
git lfs track   # 設定済みパターン一覧
git add game/
git status      # LFS 対象が "Git LFS" として識別される
```

---

## 8. よくあるハマりどころ

| 症状 | 対策 |
|------|------|
| `xcrun: error: invalid active developer path` | `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer` |
| `git lfs install` 後も普通の Git として add される | `.gitattributes` がコミットされた状態で `git add` する。先に `.gitattributes` をコミットしてから資産を add する |
| Editor 起動が極端に遅い (初回 30 分以上) | Shader コンパイル中。`Saved/Logs/` の進捗を見る。M1/M2 でも初回は時間がかかる |
| `Build failed` で Apple Silicon 向けビルドが落ちる | UE 5.5 を使っているか確認。古い 5.0/5.1 は Apple Silicon ネイティブビルドが不安定 |
| Xcode 16 で build が壊れる | UE 5.5 は Xcode 15.x が推奨。Xcode 16 で問題が出たら 15.x にダウングレード |

---

## 9. 次にやること

セットアップ完了後の最初の実装は PLAN.md の Phase 1 (UE5 Greybox):

- TPS カメラ
- ノード選択移動
- 1アリーナ
- プレイヤー、敵1種
- 射線プレビュー
- ターン進行

最初に作るべきモジュール構成や C++ クラスの初期スケッチは別 ADR で詰める。
