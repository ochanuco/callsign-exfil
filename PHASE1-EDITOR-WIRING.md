# PHASE1-EDITOR-WIRING: Phase 1 Editor 配線ガイド

## 1. 目的

Phase 1 で `main` に投入された C++ システム (Turn / Node / Camera / LoS / Combat / Pawns / HUD / PlayerController) を、UE5 Editor 側で配線して PIE で動かすまでの手順をまとめる。本書は ADR-002 と PLAN.md Phase 1 を前提とし、C++ 追加実装は行わない。Blueprint・Level・DataAsset・Input Mapping Context のセットアップ手順のみを扱う。

## 2. 完了条件 (PIE で確認できること)

- GameMode が Output Log に `[GameMode] Registered N participants` を吐く (N >= 2: プレイヤー + 敵)
- `ACallsignDebugHUD` が画面左上に `Phase=Player Current=BP_CallsignPlayer_C_0` 等の現在ターン情報を表示
- プレイヤー BP から `TryMoveToNode` を呼ぶと隣接ノードへテレポートし、ターンが進行する
- 敵が自分のターンで隣接ノードへランダムに移動する (`[EnemyAI]` ログを確認)
- LoS プレビュー線が画面に描画される (LoS が通れば緑、遮られていれば赤)

## 3. 配置するアクターと Blueprint

BP 命名規約: `BP_<C++クラス末尾>` を採用する。

| C++ クラス                           | BP 子クラス候補                           | レベル配置数             |
|-------------------------------------|------------------------------------------|--------------------------|
| `ACallsignNode`                     | `BP_CallsignNode`                        | 10〜20                   |
| `ACallsignNodeGraph`                | `BP_CallsignNodeGraph`                   | 1                        |
| `ACallsignRifleEnemy`               | `BP_CallsignRifleEnemy`                  | 1                        |
| `ACallsignExfilCharacter` (継承拡張) | `BP_CallsignPlayer` (推奨)               | 1 (`PlayerStart` 経由)   |
| `ACallsignDebugHUD`                 | (HUDClass に直接設定でも可、BP化任意)    | -                        |
| `UCallsignWeaponDefinition`         | (`UPrimaryDataAsset` インスタンス)       | -                        |
| `UInputMappingContext`              | `IMC_NodeSelect` / `IMC_Aim`             | -                        |
| `UInputAction`                      | `IA_NodeSelect` / `IA_EndTurn` / `IA_Shoot` | -                     |

`Content/CallsignPhase1/` 配下に Blueprints / DataAssets / IMC を集約する。

## 4. 手順 1: Node ビジュアル BP の作成

1. Content Browser で `Content/CallsignPhase1/` フォルダを新規作成する。サブフォルダ `Blueprints/` `Levels/` `Weapons/` `Input/` を切る。
2. `Content/CallsignPhase1/Blueprints/` で右クリック → `Blueprint Class` → All Classes から `CallsignNode` を検索 → `BP_CallsignNode` として保存。
3. `BP_CallsignNode` を開き、Components パネルで `Visual` (StaticMeshComponent) を選択。
4. Details の `Static Mesh` に `Engine.BasicShapes.Cube` を設定。
5. Transform を `Scale = (1.0, 1.0, 0.1)` (薄板) に変更。`Location.Z = 0` のまま。
6. Material は暫定で `Engine/EditorMaterials/M_TileChecker` 等を当てる (専用マテリアルがなければデフォルトで可)。
7. Class Defaults の `Attributes` を展開し、`Kind = Normal`, `HeightLevel = 0` をデフォルトに。
8. Compile → Save。

> Note: 個別ノードでは Editor 上で `Attributes.Kind` を `HalfCover` / `FullCover` / `HighGround` 等に上書きする想定。Phase 1 の遮蔽分類は LoS 判定の入力にしか使われない。

## 5. 手順 2: アリーナレベルへのノード配置

1. Editor の `Content > ThirdPerson > Maps > ThirdPersonMap` (テンプレ既定) を開く。
2. `File > Save Current Level As...` で `Content/CallsignPhase1/Levels/Lvl_Phase1Arena` として保存。
3. World Outliner で既存の地形飾り・敵プレビュー・余分なライティング以外のクラッタを削除する。床は残す。
4. `BP_CallsignNode` を Content Browser からドラッグして 10〜15 個グリッド状に配置する。間隔は `200uu` 程度を目安に。
5. 高低差検証用に 1〜2 個を `Location.Z = 200` 程度に持ち上げ、`Attributes.Kind = HighGround`, `HeightLevel = 1` に変更する。
6. `BP_CallsignNodeGraph` を 1 個配置する (位置は任意、原点近辺で良い)。`Nodes` UPROPERTY は空のまま — `BeginPlay` で自動収集される実装になっている前提でよい (実装が空であっても Phase 1 では各ノードの `Adjacent` 直接参照で動く)。
7. 各 `BP_CallsignNode` を選択 → Details の `Adjacent` 配列に隣接ノードへの参照を Editor で 1 個ずつ追加する。隣接判定は手動で行う (Phase 1 制約)。
   - グリッド配置なら、上下左右 + 必要に応じて斜めの計 4〜8 個を入れる。
   - 双方向にしたい場合は両側で参照を張る (一方向のみだと逆走できない)。
8. `Save All`。

> Tip: Adjacent 配線が膨大になりがちなので、まず 4〜6 個で動作確認してから増やす。

## 6. 手順 3: プレイヤー BP の整備

1. `Content/CallsignPhase1/Blueprints/` で `Blueprint Class` → `CallsignExfilCharacter` を検索 → `BP_CallsignPlayer` として保存。
   - 既存テンプレ `BP_ThirdPersonCharacter` を流用する場合は、Class Settings で Reparent Blueprint... → `CallsignExfilCharacter` 系に変更してから複製する。
2. `Lvl_Phase1Arena` の `PlayerStart` 付近に `BP_CallsignPlayer` を一時的にドラッグ配置する (`CurrentNode` を Editor で設定するため)。
3. 配置インスタンスを選択 → Details の `Current Node` UPROPERTY にスタートノード参照 (例: `BP_CallsignNode_0`) を設定する。これが Phase 1 のスタート位置となる。
4. PlayerStart 経由でスポーンさせる場合は、配置済み BP を消して `Default Pawn Class` を `BP_CallsignPlayer` に設定する (手順 8 参照)。代わりに BeginPlay で最近傍ノードを `CurrentNode` にバインドする小ロジックを足す (例:

   ```text
   Event BeginPlay
     -> Get All Actors Of Class (ACallsignNode) -> Nodes
     -> ForEachLoop: 自身の Location との Distance を比較し最小を保持
     -> Set CurrentNode = 最小距離のノード
   ```

   Editor 配置とどちらか一方でよい)。
5. Components パネルで既存 `SpringArm` を削除し、`Add > Callsign Shoulder Camera Component` に差し替える。Camera コンポーネントはその子に再アタッチ。
6. Compile → Save。

## 7. 手順 4: 敵 BP の整備

1. `Content/CallsignPhase1/Blueprints/` で `Blueprint Class` → `CallsignRifleEnemy` を検索 → `BP_CallsignRifleEnemy` として保存。
2. `Visual`/Mesh コンポーネントには SK_Mannequin 等の暫定スケルタルメッシュを当てる (Phase 1 はガワだけで良い)。
3. `Lvl_Phase1Arena` 上に 1 個ドラッグ配置。
4. 配置インスタンスの Details で `Current Node` を、プレイヤーから 2〜3 ノード離れた `BP_CallsignNode` インスタンスに設定する。
5. Compile → Save。

> Phase 1 の敵 AI は `ACallsignRifleEnemy::BeginTurn_Implementation` 内で隣接ノードランダム移動が走る。BehaviorTree も StateTree も Phase 1 では不要。

## 8. 手順 5: GameMode の差し替え

Phase 1 では既存テンプレ GameMode (`BP_ThirdPersonGameMode`) または `ACallsignExfilGameMode` をそのまま流用する。`BeginPlay` で `GetAllActorsWithInterface(UCallsignTurnParticipant)` を回して登録する実装は両者で共通に走る。

1. `Content/CallsignPhase1/Blueprints/` で `Blueprint Class` → `CallsignExfilGameMode` を検索 → `BP_CallsignGameMode` として保存。
2. `BP_CallsignGameMode` を開き Class Defaults で:
   - `Default Pawn Class = BP_CallsignPlayer`
   - `HUD Class = ACallsignDebugHUD` (BP 化していなければ C++ クラスを直接指定可)
   - `Player Controller Class = ACallsignExfilPlayerController` (またはその BP 派生)
3. `Lvl_Phase1Arena` を開いて `Window > World Settings`。
4. `GameMode Override = BP_CallsignGameMode` に設定。
5. `Save All`。

## 9. 手順 6: Player Controller の Mode 切替接面

`ACallsignExfilPlayerController` は `NodeSelectIMC` / `AimIMC` の 2 スロットを持つ。ターン開始時に `HandleTurnBegin` が `SetMode(NodeSelect)` を呼び、対応 IMC が自動でアクティブになる。

### 9.1 Input Mapping Context / Input Action 作成

1. `Content/CallsignPhase1/Input/` を作成。
2. `IA_NodeSelect` (`Input Action`、Value Type = `Digital (bool)`) を作成。
3. `IA_EndTurn` (`Digital (bool)`) を作成。
4. `IA_Shoot` (`Digital (bool)`) を作成。
5. `IMC_NodeSelect` (`Input Mapping Context`) を作成し、以下のマッピングを追加:
   - `IA_NodeSelect`: Mouse Left Button
   - `IA_EndTurn`: Tab Key (任意)
6. `IMC_Aim` を作成し、以下のマッピングを追加:
   - `IA_Shoot`: Mouse Left Button
   - `IA_EndTurn`: Tab Key

### 9.2 PlayerController BP

`ACallsignExfilPlayerController` を BP 化する (`BP_CallsignPlayerController`)。Class Defaults で:

- `Node Select IMC = IMC_NodeSelect`
- `Aim IMC = IMC_Aim`
- `Default Weapon = DA_DefaultPistol` (手順 7 で作成)

`BP_CallsignGameMode` の `Player Controller Class` を `BP_CallsignPlayerController` に差し替える。

### 9.3 Input Action ハンドラ (BP_CallsignPlayer or PC BP)

`BP_CallsignPlayer` (または `BP_CallsignPlayerController`) の Event Graph で各 Input Action の `Triggered` イベントを受ける:

```text
IA_NodeSelect (Triggered)
  -> Get Player Controller -> Cast to ACallsignExfilPlayerController -> PC
  -> PC.IsMyTurn -> Branch (False なら return)
  -> Get Hit Result Under Cursor By Channel (Visibility, false)
  -> Hit.Actor -> Cast to ACallsignNode -> Node
  -> PC.TryMoveToNode(Node)

IA_Shoot (Triggered)
  -> PC.IsMyTurn -> Branch
  -> Get Hit Result Under Cursor By Channel (Visibility)
  -> Hit.Actor -> PC.TryShootAtActor(Hit.Actor)

IA_EndTurn (Triggered)
  -> PC.EndTurn
```

`BP_CallsignPlayerController` の `Event BeginPlay` で `Set Show Mouse Cursor = true` を呼ぶ (ノードクリック検出用)。

### 9.4 Camera コンポーネントの確認

`BP_CallsignPlayer` の Components で `UCallsignShoulderCameraComponent` が SpringArm の代替になっていること。`SetCameraMode` 呼び出しは Phase 1 では `OnControllerModeChanged` をバインドして自動同期するか、手動で IA から呼ぶ。BP 接続例:

```text
PC.OnControllerModeChanged (NewMode)
  -> ShoulderCamera.SetCameraMode(NewMode を ECallsignCameraMode に Map)
```

## 10. 手順 7: DefaultWeapon DataAsset 作成

1. `Content/CallsignPhase1/Weapons/` フォルダを作成。
2. 右クリック → `Miscellaneous > Data Asset` → `CallsignWeaponDefinition` を選択 → `DA_DefaultPistol` として保存。
3. Details で:
   - `Damage = 25`
   - `Range = 2000`
   - `b Requires Line Of Sight = true`
4. `BP_CallsignPlayerController` の Class Defaults の `Default Weapon` スロットに `DA_DefaultPistol` を設定する。
5. PlayerController を BP 化していない場合は、`ACallsignExfilPlayerController` C++ クラスのデフォルト値を Editor で編集できないため、必ず手順 9.2 の BP 化を先に行うこと。

## 11. 手順 8: PIE で確認

1. `Lvl_Phase1Arena` を開いて `Play in Editor` (Alt+P)。
2. `Output Log` (Window > Output Log) で以下を順に確認:
   - `[Turn] Subsystem initialized`
   - `[GameMode] Registered N participants` (N >= 2)
   - `[Turn] BeginRound, N participants`
   - `[Turn] OnTurnBegin BP_CallsignPlayer_C_0` (登録順 = `GetAllActorsWithInterface` 走査順)
   - `[PC] HandleTurnBegin -> NodeSelect`
3. 画面左上に DebugHUD のターン情報テキストが出ていること (`Phase=Player Current=BP_CallsignPlayer_C_0` 風)。
4. マウスを画面上で動かすと、プレイヤー位置からカーソル方向への LoS プレビュー線 (緑/赤) が描画される。
5. `BP_CallsignNode` の上でクリック → `[Pawn] BP_CallsignPlayer_C_0 MoveToNode -> BP_CallsignNode_3` のログとプレイヤー位置の更新 → 自動で `EndTurn` → 敵ターンへ遷移。
6. 敵ターンで `[EnemyAI] BP_CallsignRifleEnemy_C_0 picked node BP_CallsignNode_X` ログ → 敵が隣接ノードへ移動。
7. 敵ターン終了 → `[Turn] OnTurnBegin BP_CallsignPlayer_C_0` でラウンド一巡。

すべてのログが出て移動も視認できれば Phase 1 完了条件 (PLAN.md 587-591行) を満たす。

## 12. 既知の Phase 1 制約 (期待しないこと)

- 隣接ノードの自動算出はない。手動で `Adjacent` UPROPERTY を配線する。
- 武器破損・リロード・支援要請・抽出フェーズは Phase 2+ で扱う。Phase 1 の `UCallsignWeaponDefinition` には `Damage` / `Range` / `bRequiresLineOfSight` しかない。
- LoS の部分遮蔽は未実装。`bHasLineOfSight` は full block / no block の二択。
- 移動の補間アニメーションはない。Phase 1 は瞬間移動 (テレポート)。スムーズ補間は Phase 2 のキャラクタ移動で扱う。
- `ECallsignTurnPhase` enum は実装上 `Player` で固定的に流れる。Player vs Enemies の厳密な相分離は将来の Phase で扱う。
- BehaviorTree / StateTree の敵 AI は未実装。`ACallsignRifleEnemy::BeginTurn_Implementation` の C++ ロジックのみ。
- DataTable 経由の敵ステータス読み込みは Phase 1 では未配線でよい。`FCallsignEnemyStats` の枠だけある。

## 13. トラブルシュート

### `[GameMode] Registered 0 participants`

ノードは配置されているが、プレイヤー/敵の BP が `ICallsignTurnParticipant` を継承していない可能性が高い。

- `BP_CallsignPlayer` の親が `ACallsignExfilCharacter` か (`Class Settings > Parent Class`)。
- `BP_CallsignRifleEnemy` の親が `ACallsignRifleEnemy` か。
- 既存 `BP_ThirdPersonCharacter` を Reparent している場合、Reparent 後に再コンパイルしているか。

### HUD 表示が出ない

- `BP_CallsignGameMode` の `HUD Class` が `ACallsignDebugHUD` (または BP 派生) になっているか。
- `Lvl_Phase1Arena` の World Settings で `GameMode Override` が `BP_CallsignGameMode` になっているか。
- BP HUD を作成した場合、親が `ACallsignDebugHUD` になっているか。
- `bShowTurnInfo = true`, `bShowLoSPreview = true` が Class Defaults で生きているか。

### `TryMoveToNode` が `false` を返す

- `IsMyTurn()` が `true` を返しているか (Output Log で現在ターン参加者を確認)。
- プレイヤーの `CurrentNode` が `nullptr` でないか (Editor で設定 or BeginPlay で最近傍検索)。
- `Target->Adjacent` 配列に対象ノードが含まれているか (`BP_CallsignNode` 個別の Details で確認)。
- 双方向に張る必要がある場合、両ノードで相互に追加しているか。

### マウスクリックがノードに当たらない

- `BP_CallsignPlayerController` で `Set Show Mouse Cursor = true` を BeginPlay で呼んでいるか。
- BP graph で `Get Hit Result Under Cursor By Channel(ECollisionChannel::ECC_Visibility, false, Hit)` を呼び、`Hit.GetActor()` が `BP_CallsignNode` か確認。
- `BP_CallsignNode` の `Visual` (StaticMeshComponent) の Collision プリセットが `BlockAllDynamic` 等、Visibility トレースに反応するものになっているか。
- スケール `(1.0, 1.0, 0.1)` で薄すぎる場合は当たり判定範囲が狭くなりがち。一時的に `0.2` 程度に厚くして検証する。

### LoS プレビュー線が出ない

- `ACallsignDebugHUD::bShowLoSPreview` が `true` か。
- `UCallsignLineOfSightService` World Subsystem が初期化されているか (`[LoS] Subsystem initialized` 等のログ)。
- カメラから前方トレースが World Static に当たっているか (空に向けると線が描画されない実装の可能性あり)。

## 14. 次のステップ (Phase 1 後半 / Phase 2 前半)

- PLAN.md Phase 1 完了条件を再確認し、満たしていれば Phase 2 設計 ADR (ADR-003 想定) を起こす。
- Phase derivation の本実装: Player vs Enemies フェーズの厳密分離、Resolving フェーズの再導入。
- 武器耐久・弾薬・マガジン・リロード・タクティカルリロード (Phase 2 / PLAN.md 595-614行)。
- StateTree ベース敵 AI への移行 (Melee / Heavy / Jammer の追加に備える)。
- LoS の部分遮蔽 (CoverLevel) 反映、ダメージ減衰式の実装。
- ノード隣接の自動算出 (距離ベース or Editor utility)。
- 抽出フェーズ・支援要請・地形破壊は Phase 3 / 4 で扱う。
