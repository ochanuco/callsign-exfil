# ADR-002: Phase 1 (UE5 Greybox) モジュール構造

## Status

Accepted

## Context

`PLAN.md` の Phase 1 (UE5 Greybox) では、肩越しTPSカメラ・ノード選択移動・1アリーナ・プレイヤー・敵1種・射線プレビュー・射撃・ターン進行を成立させ、肩越し視点でのノード選択が破綻しないこと、射線と遮蔽が読めることを完了条件とする。本 ADR はこの Phase 1 範囲に限定し、C++ モジュール分割、主要クラス分担、C++ と Blueprint の配分、依存方向、命名規約、データ駆動境界を確定する。Phase 2 以降の武器破損・リロード・支援要請・抽出フェーズには踏み込まない。

ベースは UE5.7 Third Person C++ テンプレート (`game/CallsignExfil/`)。既存クラスは `ACallsignExfilCharacter` / `ACallsignExfilGameMode` / `ACallsignExfilPlayerController` の3点であり、`Plugins` として `StateTree` / `GameplayStateTree` / `ModelingToolsEditorMode` が有効化されている。Phase 1 の追加実装は単一 Runtime モジュール `CallsignExfil` 内のサブシステム分割で扱う。

## Decision

### 1. Phase 1 スコープ確認

Phase 1 で実装するのは以下に限る。

- 肩越しTPSカメラ
- ノード選択移動 (1ターン1ノード)
- 手作りグレーボックスアリーナ1個
- プレイヤーキャラクター
- Rifle Enemy 相当の敵1種
- 射線プレビュー
- 射撃 (命中確定寄り、簡易ダメージのみ)
- ターン進行 (Player → Enemies → Player)

完了条件は `PLAN.md` の Phase 1 セクションに従う。

### 2. モジュール分割

UE プロジェクトとしての C++ モジュールは Phase 1 では `CallsignExfil` 単一を維持する。複数モジュール (`.Build.cs` の追加) は Phase 1 では切らない。代わりに、ソースツリー上のサブディレクトリで論理サブシステムを分離する。

`game/CallsignExfil/Source/CallsignExfil/` 配下に以下を配置する。

- `Core/` — ゲームモード、プレイヤーコントローラ、グローバルな型・ログ宣言
- `Turn/` — ターン進行サブシステム
- `Node/` — ノードグラフとノード単体
- `Camera/` — 肩越しTPSカメラ制御
- `LineOfSight/` — 射線判定とプレビュー
- `Combat/` — 射撃解決とダメージ適用
- `Pawns/` — プレイヤーポーン、敵ポーン、AI
- `Data/` — DataTable / DataAsset 用 USTRUCT 宣言

各サブシステムの owned concern は次のとおり。

| サブシステム  | Owned concern                                                                       |
| ------------- | ----------------------------------------------------------------------------------- |
| Core          | GameMode による全体ライフサイクル、PlayerController による入力ルーティング          |
| Turn          | ターンキューの保持、現在アクター決定、ターン終了通知のブロードキャスト              |
| Node          | ノードの位置・属性・隣接情報、グラフ全体の所有、占有アクターの追跡                  |
| Camera        | 肩越しオフセット、ノード選択中のカメラ振る舞い、エイム時のオフセット切替            |
| LineOfSight   | 2点間の射線可否判定、遮蔽分類、UI 用プレビュー結果の生成                            |
| Combat        | 射撃アクションの解決、ダメージ算出、ヒット通知                                      |
| Pawns         | プレイヤー/敵の見た目と移動再生、敵 AI のターン内意思決定                           |
| Data          | ノード属性、敵ステータス、武器ステータスの USTRUCT 定義                             |

### 3. クラス分担スケッチ

UE プレフィックス規約 (A=Actor派生 / U=UObject 派生 / F=plain struct / I=interface) に従う。Phase 1 で導入する主要クラスは以下とする。実装は書かない。

#### 3.1 Core

> `ACallsignExfilGameMode` (継承拡張): Phase 1 用にターンサブシステム生成と初期スポーンを担当する。

> `ACallsignExfilPlayerController`: 入力をターンサブシステムへ委譲する。情報確認系入力 (視点、射線プレビュー) はターンを消費しないルートに分ける。

#### 3.2 Turn

> `UCallsignTurnSystem : UWorldSubsystem`: ターンキューを保持し、現在のターンアクターを公開する。`OnTurnBegin` / `OnTurnEnd` をブロードキャストする。

> `ICallsignTurnParticipant`: ターンに参加するアクターが実装するインターフェース。`BeginTurn()` / `IsTurnFinished() const` を要求する。

> `ECallsignTurnPhase`: `Player` / `Enemies` / `Resolving` の enum。

#### 3.3 Node

> `ACallsignNodeGraph`: アリーナ内に1個配置する所有アクター。子ノードをまとめて保持し、隣接クエリを提供する。

> `ACallsignNode`: ワールド配置可能な単一ノード。座標、属性、占有者、隣接ノード参照を持つ。

> `UCallsignNodeOccupancy`: ノード占有状態 (空 / プレイヤー / 敵 / 障害) を表す軽量 UObject または USTRUCT。Phase 1 では `FCallsignNodeOccupancy` USTRUCT とする。

> `ICallsignNodeOccupant`: ノードを占有しうるアクターが実装する。`GetCurrentNode() const` / `MoveToNode(ACallsignNode*)` を要求する。

#### 3.4 Camera

> `UCallsignShoulderCameraComponent : USpringArmComponent`: 肩越しオフセットと、ノード選択中の俯瞰寄り、エイム中のタイト寄りを切り替える。

#### 3.5 LineOfSight

> `UCallsignLineOfSightService : UWorldSubsystem`: 2点 (またはノード間) で射線可否と遮蔽強度を返す。物理クエリを集約する。

> `FCallsignLineOfSightResult`: `bHasLineOfSight` / `CoverLevel` / `BlockingActor` を持つ POD。

#### 3.6 Combat

> `UCallsignCombatResolver : UWorldSubsystem`: 射撃アクションを受け取り、LineOfSight サービスへ問い合わせ、ダメージを適用する。Phase 1 ではヒットなら固定ダメージで構わない。

> `FCallsignShotRequest` / `FCallsignShotResult`: 射撃要求と結果の POD。

#### 3.7 Pawns

> `ACallsignExfilCharacter` (既存拡張): `ICallsignNodeOccupant` / `ICallsignTurnParticipant` を実装する。

> `ACallsignRifleEnemy : ACharacter`: Phase 1 唯一の敵種。`ICallsignNodeOccupant` / `ICallsignTurnParticipant` を実装する。

> `UCallsignRifleEnemyController : AAIController` (実体は AAIController 派生): ターン中に隣接ノードへの移動または射撃を選ぶ。Phase 1 では Behavior Tree でなく単純な C++ ロジック、または StateTree のごく簡易グラフで構わない。

#### 3.8 Data

> `FCallsignNodeAttributes` USTRUCT: ノード種別 (Normal / HalfCover / FullCover / HighGround / LowGround / Indoor) と高さインデックスを持つ。

> `FCallsignEnemyStats` USTRUCT: HP、射程、ダメージ、行動優先度。

> `UCallsignWeaponDefinition : UPrimaryDataAsset`: Phase 1 では「ハンドガン的な単発射撃武器」1種のみ定義する。耐久・弾薬・マガジン関連フィールドは持たない。

### 4. C++ vs Blueprint 配分ポリシー

判定ロジック・状態機械・ターン制御・射線判定・ダメージ計算・AI 意思決定は C++ で書く。これらは決定論的で単体テスト可能であるべきであり、Blueprint の差分を追うとルール検証が困難になるためである。一方、見た目・アニメーション・カメラの数値調整 (Spring Arm length など)・UMG ウィジェット・ノードのビジュアルマーカー・アリーナのレベル配置・敵スポーン位置・初期パラメータの試行錯誤は Blueprint および Editor で扱う。Phase 1 ではすべての C++ クラスを `BlueprintType` または `Blueprintable` で公開し、デザイナがイテレートできる接面を確保する。

### 5. 依存方向

依存方向は次の DAG とする。循環依存は禁止する。

```
                     +--------+
                     |  Core  |
                     +---+----+
                         |
        +----------------+----------------+
        v                v                v
   +---------+      +--------+       +-------+
   |  Turn   |      | Pawns  |<----->| Camera|
   +----+----+      +---+----+       +-------+
        |               |
        |               v
        |          +---------+
        |          |  Node   |
        |          +----+----+
        |               |
        v               v
   +---------+     +-------------+
   | Combat  |---->| LineOfSight |
   +----+----+     +------+------+
        |                 |
        v                 v
   +-------------------------+
   |          Data           |
   +-------------------------+
```

ルール:

- `Data` は他に依存しない (純粋な型定義)。
- `LineOfSight` は `Data` のみ参照する。
- `Combat` は `LineOfSight` と `Data` を参照する。
- `Node` は `Data` のみ参照する。Pawn 側の具象型を知らない。
- `Pawns` は `Node` と `Camera` と `Turn` のインターフェースに依存する。具象 Combat ロジックは PlayerController 経由で呼ぶ。
- `Turn` は参加者をインターフェース (`ICallsignTurnParticipant`) 越しにのみ知る。
- `Core` のみが上記すべてを束ねる。

`Camera` と `Pawns` の双方向は、`Camera` が Pawn にアタッチされる構造上避けがたいため、コンポーネント化により片方向 (Camera は Pawn に所属する `USceneComponent` 派生) として扱う。

### 6. 命名規約

既存テンプレートが `CallsignExfil` プレフィックスを使っているが、Phase 1 で追加する新規クラスはやや短縮した `Callsign` プレフィックスを採用する。理由は、`CallsignExfil` を毎回打つと冗長で、サブシステム名が読みにくくなるためである。既存 3 クラス (`ACallsignExfilCharacter` 等) はテンプレート由来の名前であるためそのまま残し、改名はしない。

採用ルール:

- 新規ゲームプレイクラス: `Callsign` プレフィックス + UE 型プレフィックス。
  - 例: `UCallsignTurnSystem`, `ACallsignNodeGraph`, `ACallsignNode`, `UCallsignShoulderCameraComponent`, `UCallsignLineOfSightService`, `UCallsignCombatResolver`, `ACallsignRifleEnemy`
- インターフェース: `ICallsignXxx` (例: `ICallsignTurnParticipant`, `ICallsignNodeOccupant`)
- POD/USTRUCT: `FCallsignXxx` (例: `FCallsignShotResult`, `FCallsignLineOfSightResult`)
- enum: `ECallsignXxx` (例: `ECallsignTurnPhase`)
- 既存テンプレート由来クラスは `CallsignExfil` プレフィックスを維持する。

ファイル名はクラス名と一致させる (`CallsignTurnSystem.h` など)。

### 7. データ駆動の境界

Phase 1 で扱う3種のデータは以下のように配分する。

- **ノード属性**: ノードはレベル配置の `ACallsignNode` インスタンス上の `UPROPERTY` (`FCallsignNodeAttributes`) で持つ。DataTable 化はしない。理由は、Phase 1 のアリーナは1個のみであり、ノードごとに個別チューニングする方が早いためである。
- **敵ステータス**: `UDataTable` + `FCallsignEnemyStats` の組で扱う。RowName で参照する。Phase 1 で敵は1種のみだが、後続フェーズで増えるためテーブルから始める。
- **武器ステータス**: `UCallsignWeaponDefinition : UPrimaryDataAsset` で1個だけ作る。Phase 1 では耐久・弾薬・マガジンを実装しないため、`Damage` / `Range` / `bRequiresLineOfSight` 程度に絞る。

DataTable 経由で読み込む処理は `Core` の GameMode で行い、サブシステムへ注入する。

### 8. Phase 1 で意図的に保留する項目

以下は Phase 1 では扱わない。クラスもプロパティも作らない。

- 武器耐久、武器破損、摩耗
- 弾薬、マガジン、リロード、タクティカルリロード
- ニコイチ、分解、武器交換、武器持ち替え
- 支援要請、遅延発動、地形破壊、巻き込み判定
- 救済ハンドガンの専用ロジック (Phase 1 武器1種で兼ねる)
- 抽出フェーズ、警戒レベル、増援
- BP 断片、戦闘ログ、初期支給選択、セーフティゾーン
- 敵 AI の高度な意思決定 (Behavior Tree の本実装、視覚知覚)
- 自動生成マップ、ノードのランダム配置
- 弾種、ダメージタイプ、状態異常

これらは Phase 2 以降で別 ADR として扱う。Phase 1 のクラスにこれらのフィールドを先取りで追加することも禁止する。

### 9. Open Questions

Phase 1 実装中に検証・確定が必要な点:

1. ノード隣接の保持方法を、`ACallsignNode` 上の `TArray<TObjectPtr<ACallsignNode>>` にするか、`ACallsignNodeGraph` 上の隣接リストにするか。エディタでの編集容易性とランタイム参照のしやすさが衝突する。
2. ターンサブシステムを `UWorldSubsystem` にするか `UGameInstanceSubsystem` にするか。シーン跨ぎがない Phase 1 では前者で十分だが、後続フェーズで作戦マップとアリーナを分けたとき影響が出る。
3. 肩越しカメラのノード選択モードと射撃モードの遷移を、入力モード (`InputMappingContext` の切替) で表現するか、PlayerController 内のステートで表現するか。
4. 敵 AI を C++ 直書きにするか、StateTree (`GameplayStateTree` プラグインが既に有効) を採用するか。Phase 1 1種のみであれば C++ で済むが、将来の Melee/Heavy/Jammer のために StateTree を最初から使う選択もある。
5. `ICallsignNodeOccupant` を Pawn 全般に持たせるか、コンポーネント (`UCallsignNodeOccupantComponent`) に切り出すか。Player と Enemy で実装が重複しそうなため、コンポーネント化が妥当だがクラス数が増える。

## Consequences

### Positive

- 単一モジュール内のサブディレクトリ分割により、Phase 1 段階で `.uproject` の Module 追加とビルド構成変更を避けられる。
- サブシステム境界とインターフェース境界が明示されているため、Phase 2 以降に Combat / Turn を拡張する際の影響範囲を局所化できる。
- C++ と Blueprint の配分が明文化されており、判定ロジックが Blueprint へ漏れる事故を抑止できる。
- 命名プレフィックスを `Callsign` に短縮することで、新規クラス名の可読性を確保できる。

### Negative

- `Callsign` と `CallsignExfil` の二重プレフィックスが当面共存する。
- サブシステムを `UWorldSubsystem` に寄せたため、PIE 起動順や WorldSubsystem 初期化順序に依存する初期化バグを引き当てる可能性がある。
- ノード隣接をレベル配置で持たせると、グラフ整合性チェックを Editor 拡張で書く必要が後で出る。

### Risks

- Phase 1 で「保留」と決めた項目をクラス設計で先取りしたくなる誘惑がある。逸脱が起きた場合は本 ADR との照合で差し戻すこと。
- 肩越しカメラとノード選択の相性が完了条件 (`PLAN.md` Phase 1) を満たさない場合、`Camera` サブシステムの再設計が必要になり、本 ADR の Camera 記述は再検討対象となる。
