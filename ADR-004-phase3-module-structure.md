# ADR-004: Phase 3 (Support Prototype) モジュール構造

## Status

Accepted

## Context

`PLAN.md` の Phase 3 (Support Prototype) では、支援要請2〜3種・範囲予告・遅延発動・敵味方巻き込み・地形破壊・支援カウントUI を実装し、(a) 支援要請が強いが雑に使えない、(b) 自爆や地形破壊が納得できる、(c) 支援待ちのターンが面白い、を完了条件とする。Phase 1 (TPS+Turn+Node+LoS+Combat) と Phase 2 (Inventory+Weapon リソース管理) で確立した `CallsignExfil` 単一モジュール + サブディレクトリ分割の上に、遅延発動 AoE である「支援要請」層を新規に乗せる。

本 ADR は Phase 3 範囲に限定し、支援要請の C++ サブシステム構造、Turn 進行との結合、UI/プレビュー接面、Combat への damage routing、`ACallsignNode` の破壊フラグ拡張を確定する。`ADR.md` §11-12 の支援設計を C++ に写し、PLAN.md の MVP 支援3種 (Precision Strike / Supply Pod / Orbital Barrage) を Data Asset 駆動で表現する。

ニコイチ (Combine) / 分解 (Disassemble) / 拾った武器の操作 / 武器特性継承は Phase 4 別 ADR で扱う。**ADR-003 §12 はこれらを「Phase 3 で別 ADR 化」と記載していたが、本 ADR で Phase 4 へ訂正する**。Phase 3 は支援要請の検証に集中し、武器資源の追加機能は持ち込まない。

## Decision

### 1. Phase 3 スコープ確認

Phase 3 で実装するのは以下に限る。

- 支援要請3種 (Precision Strike / Supply Pod / Orbital Barrage)
- 範囲予告 (爆心地・半径の可視化、Phase 3 では `DrawDebugSphere` ベース)
- 遅延発動 (`DelayTurns` ターン後に自動発動、Turn システムと結合)
- 敵味方巻き込み (Combat Resolver 経由でダメージ送信、IFF なし)
- 地形破壊 (`ACallsignNode` の `bDestroyable` ノードを `bIsDestroyed=true` にする簡易実装)
- 支援カウント UI (Phase 1 の `ACallsignDebugHUD` を拡張、UMG 化は Phase 4+)

完了条件は `PLAN.md` の Phase 3 セクションに従う。

ADR-003 §12 で「Phase 3 で別 ADR 化」とされていたニコイチ・分解・武器特性継承・拾得操作は **Phase 4 別 ADR へ移行する**。本 ADR では一切扱わない (詳細は §12)。

### 2. モジュール分割 (Phase 1+2 構造への拡張)

UE プロジェクトとしての C++ モジュールは Phase 3 でも `CallsignExfil` 単一を維持する。Phase 1+2 で確立したサブディレクトリ分割の方針を継続する。

`game/CallsignExfil/Source/CallsignExfil/` 配下に Phase 3 で以下を追加する。

- `Support/` 新設 — 支援要請キュー、支援定義、支援発動 Resolver
- `Combat/` (既存) — Phase 3 での変更なし。支援は既存 `UCallsignCombatResolver` を呼び出す側に立つ
- `Node/` (既存) — `ACallsignNode` に破壊フラグを追加。隣接削除ロジックを追加
- `HUD/` (既存) — `ACallsignDebugHUD` に支援 pending 表示と範囲予告 (DrawDebugSphere) を追加
- `Data/` (既存) — `ECallsignSupportType` / `ECallsignSupportPhase` 等の enum と `FCallsignSupportRequest` POD を追加
- `Inventory/` / `Weapon/` (既存) — Phase 3 では一切触らない

各サブシステムの owned concern は次のとおり。

| サブシステム | Owned concern                                                                                            |
| ------------ | -------------------------------------------------------------------------------------------------------- |
| Support      | 支援要請の永続キュー、ターン経過に伴う `TurnsRemaining` 減算、発動タイミングの判定                       |
| Support (Resolver) | 単一支援の発動処理 (範囲内 Pawn の収集、Combat 経由のダメージ送信、Destroyable Node の破壊)        |
| Combat       | (既存) 単発射撃の解決。Phase 3 では支援からの呼出経路が追加されるが、Resolver 内部の処理は変更しない     |
| Node         | (既存) ノードの位置・属性・隣接情報。Phase 3 で `bDestroyable` / `bIsDestroyed` を追加し破壊状態を保持   |
| HUD          | (既存) Phase 3 で支援 pending リストの画面表示と、範囲予告球の World 描画を追加                          |
| Data         | (既存) Phase 3 で支援系 enum / POD / Data Asset を追加                                                   |

### 3. データ層

#### 3.1 新規 USTRUCT / enum

以下を `Data/CallsignSupportTypes.h` (新設) に追加する。Phase 1+2 既存型は変更しない。

- `ECallsignSupportType` — `PrecisionStrike` / `SupplyPod` / `OrbitalBarrage`
- `ECallsignSupportPhase` — `Pending` / `Resolving` / `Resolved` / `Cancelled` (キュー上の状態 enum)
- `FCallsignSupportRequest` USTRUCT (POD) — `RequestId` (FGuid) / `RequestedBy` (TWeakObjectPtr<AActor>) / `TargetLocation` (FVector) / `Definition` (TObjectPtr<UCallsignSupportDefinition>) / `TurnsRemaining` (int32) / `Phase` (ECallsignSupportPhase)
- `FCallsignSupportResolution` USTRUCT (POD) — `RequestId` (FGuid) / `DamageEventsEmitted` (int32) / `DestroyedNodeIds` (TArray<FGuid> または TArray<TWeakObjectPtr<ACallsignNode>>) / `bFriendlyFireApplied` (bool)
- `UCallsignSupportDefinition : UPrimaryDataAsset` — Phase 2 の `UCallsignWeaponDefinition` と同じく静的データ Asset として扱う。フィールドは以下:
  - `SupportType` (`ECallsignSupportType`)
  - `DelayTurns` (int32, 1〜5)
  - `RadiusCm` (float, ダメージ範囲半径)
  - `Damage` (int32)
  - `TerrainDestructionRadiusCm` (float, ノード破壊半径。0 で破壊なし)
  - `bAllowsFriendlyFire` (bool, Phase 3 では常に true で運用するが、Phase 4 IFF 拡張用に枠を確保)
  - `bDealsDamage` (bool, SupplyPod の挙動を Phase 3 で切り替えるためのフラグ。§13 OQ-4 参照)

#### 3.2 既存型への拡張

`ACallsignNode` に以下の `UPROPERTY` を追加する。`FCallsignNodeAttributes` 側ではなくノード Actor 上に直接持たせる (§11 参照)。

- `bDestroyable` (bool, EditAnywhere) — Editor で配置時に決める静的フラグ
- `bIsDestroyed` (bool, VisibleAnywhere, Transient) — ランタイム破壊状態

既存 Phase 1 型 (`FCallsignNodeAttributes`) は変更しない。Phase 3 のノード破壊フラグはノード単体の instance state であり、共有可能な属性 (Normal / HalfCover 等) とは責務が異なるため、`FCallsignNodeAttributes` ではなく `ACallsignNode` 上の独立 `UPROPERTY` で扱う。

### 4. クラス分担スケッチ

UE プレフィックス規約 (ADR-002 §6 / ADR-003 §10) に従う。Phase 3 で導入する主要クラスは以下とする。実装は書かない。

#### 4.1 Support

> `UCallsignSupportSystem : UWorldSubsystem`: 支援要請の永続キューを所有する。プレイヤー/敵 (将来) からの `SubmitRequest` を受け、Turn 終端ごとに `TurnsRemaining` を減算し、0 到達時に `UCallsignSupportResolver::Resolve` を呼出す。
>
> 公開 API (BlueprintCallable):
>
> - `RequestSupport(ECallsignSupportType, const FVector& TargetLocation, AActor* RequestedBy)` → `FGuid` (request id)
> - `GetPendingRequests() const` → `TArray<FCallsignSupportRequest>`
> - `CancelRequest(FGuid RequestId)` → `bool`
> - `OnSupportSubmitted` (delegate, `FCallsignSupportRequest`)
> - `OnSupportResolved` (delegate, `FCallsignSupportResolution`)
>
> Turn 結合: `Initialize` で `UCallsignTurnSystem::OnTurnEnd` を購読する。各 turn end 時に Phase=Pending の全 request の `TurnsRemaining` を 1 減らし、0 到達ぶんを `UCallsignSupportResolver` に渡す。

#### 4.2 Support (Resolver、`Support/` サブディレクトリ内)

> `UCallsignSupportResolver : UWorldSubsystem`: 個別の支援発動処理を担う。`Resolve(const FCallsignSupportRequest&)` を呼ばれて以下を実行する。
>
> 1. `OverlapMultiByObjectType(ECC_Pawn)` で範囲内 Pawn を集める
> 2. 各 Pawn に対し Combat resolver 経由でダメージを送信 (友軍含む、IFF なし)
> 3. ワールド内の `ACallsignNode` のうち、爆心地から `TerrainDestructionRadiusCm` 内かつ `bDestroyable=true` のものを `bIsDestroyed=true` に切替
> 4. 破壊された Node を全 Adjacent から除去 (片方向)
> 5. 副作用を `FCallsignSupportResolution` にまとめ、`UCallsignSupportSystem::OnSupportResolved` 経由で broadcast
>
> 公開 API (BlueprintCallable):
>
> - `Resolve(const FCallsignSupportRequest& Request)` → `FCallsignSupportResolution`

`UCallsignSupportSystem` と分離する理由は、(a) キュー管理 (時間軸) と発動処理 (空間軸) の責務分離、(b) Phase 4 でジャミング (`JammerEnemy` による発動阻害) を System 側に追加する際、Resolver は変えずに済む構造を確保するため (§Risks 参照)。

#### 4.3 PlayerController 拡張

> `ACallsignExfilPlayerController` に新規 BlueprintCallable: `TryRequestSupport(ECallsignSupportType Type, ACallsignNode* AtNode)` を追加する。Phase 3 では「ターン消費 1」「常時利用可」「数量制限なし」とし、Phase 2 の `TryShoot` / `TryReload` と同じ接面 (Player ターンチェック → SupportSystem へ委譲 → Turn 消費) を踏襲する。
> 内部処理:
> 1. `IsMyTurn` 確認
> 2. `AtNode` から target location を取得
> 3. `UCallsignSupportSystem::RequestSupport` 呼出
> 4. `UCallsignTurnSystem::EndCurrentTurn` でターン消費

#### 4.4 HUD/Preview 拡張

> `ACallsignDebugHUD` (Phase 1) を Phase 3 で拡張する。UMG への移行は Phase 4+。
>
> - `DrawHUD` 内で `UCallsignSupportSystem::GetPendingRequests` を取得し、画面左下に各 pending の `[Support N] PrecisionStrike 2T -> (X,Y,Z)` 形式テキストを描画
> - `BeginPlay` で `UCallsignSupportSystem` を解決し、`OnSupportResolved` を購読 (発動結果ログを画面に短時間表示)
>
> 範囲予告: `ACallsignDebugHUD::DrawHUD` から毎フレーム `DrawDebugSphere(World, request.TargetLocation, request.Definition->RadiusCm, segments, color)` を全 pending について発行する。`TurnsRemaining == 0` (今ターン中に発動予定) のものは色を変える。

UMG ベースの予告ウィジェットは Phase 4+ 案件。Phase 3 完了条件 (a)(b)(c) を満たす最小限の可視化として World 空間 DebugSphere を許容する。

### 5. C++ vs Blueprint 配分ポリシー

Phase 1+2 の方針 (ADR-002 §4 / ADR-003 §5) を踏襲する。Phase 3 で追加する責務の配分は以下。

- C++: 支援キュー管理、ターン進行との結合、Resolve 処理 (Pawn 収集・ダメージ送信・ノード破壊)、`bAllowsFriendlyFire` の判定 (Phase 3 では常に true 経路に短絡)
- Data Asset (Editor): `UCallsignSupportDefinition` を 3 個 (PrecisionStrike / SupplyPod / OrbitalBarrage) 作成。`DelayTurns` / `RadiusCm` / `Damage` / `TerrainDestructionRadiusCm` / `bAllowsFriendlyFire` / `bDealsDamage` を Editor で調整可能にする
- Blueprint / UMG: HUD 演出、SE/VFX、爆心地マーカーの最終アート、爆発演出の Niagara
- 範囲予告: Phase 3 では C++ の `DrawDebugSphere` で済ませる。UMG/Niagara への置換は Phase 4+

判定ロジックが Blueprint へ漏れると支援ルールの検証が困難になるため、キュー進行・発動判定・巻き込み判定は必ず C++ で完結させる。

### 6. 依存方向

Phase 1+2 の DAG (ADR-003 §6) を維持しつつ、`Support` を追加する。循環依存は禁止する。

```text
                     +--------+
                     |  Core  |
                     +---+----+
                         |
        +----------------+----------------+
        v                v                v
   +---------+      +--------+       +-------+
   |  Turn   |      | Pawns  |------>| Camera|
   +----+----+      +---+----+       +-------+
        |               |
        |               v
        |          +-----------+
        |          |   Node    |
        |          +-----+-----+
        |                |
        v                v
   +---------+   +---------------+
   | Combat  |-->|   LineOfSight |
   +----+----+   +-------+-------+
     |  ^               |
     |  |               v
     |  +-----------+   +-----------+
     |  |  Support  |-->|   Data    |
     |  +-----------+   +-----------+
     v
   +-----------+
   | Inventory |
   |  Weapon   |
   +-----------+
        (Support は Combat 経由で間接的に Inventory/Weapon を利用するが、
         Support → Inventory/Weapon の直接依存は持たせない)
```

ルール:

- `Data` は他に依存しない (純粋な型定義)
- `Support` は `Combat` (ダメージ送信) / `Node` (破壊フラグ書換) / `Turn` (`OnTurnEnd` 購読) / `Data` を参照する
- `Support` は `Inventory` / `Weapon` を **参照しない** (Phase 3 では支援が武器インスタンスを介さないため)
- `Combat` は Phase 2 の構造を変えない (`Inventory` / `Weapon` / `Data` / `LineOfSight` 参照)
- `Node` は `Data` のみ依存。Support 側からの破壊書換は Support → Node の片方向呼出
- `Turn` は引き続き `ICallsignTurnParticipant` 越しにのみ参加者を知る。`OnTurnEnd` delegate は Support 側が購読する側
- `Core` のみが上記すべてを束ねる

`Support → Combat` 依存方向は新規。Combat は Support の存在を知らない (片方向)。

### 7. 支援要請ライフサイクル

支援要請の 1 サイクルは以下の番号順で進む。

1. PC (`ACallsignExfilPlayerController::TryRequestSupport`) が `ECallsignSupportType` と `ACallsignNode*` を受ける
2. PC が `UCallsignSupportSystem::RequestSupport(Type, NodeLocation, Self)` を呼び、`FGuid` を受ける
3. SupportSystem が新 `FCallsignSupportRequest` (Phase=Pending, TurnsRemaining=Definition->DelayTurns) をキューに追加し、`OnSupportSubmitted` を broadcast
4. PC が `UCallsignTurnSystem::EndCurrentTurn` を呼び、Player ターン消費
5. SupportSystem が `OnTurnEnd` 購読中で、各 turn end ごとに Phase=Pending の全 request の `TurnsRemaining` を 1 減らす
6. `TurnsRemaining == 0` に到達した request について、Phase を `Resolving` に切替、`UCallsignSupportResolver::Resolve(req)` を呼出
7. Resolver が範囲内 Pawn を `OverlapMultiByObjectType(ECC_Pawn)` で集め、各 Pawn に対し Combat 経由でダメージ送信
8. Resolver が範囲内 `ACallsignNode` のうち `bDestroyable=true` のものを `bIsDestroyed=true` に切替、Adjacent から除去
9. Resolver が `FCallsignSupportResolution` を返却、SupportSystem が Phase を `Resolved` に切替、`OnSupportResolved` を broadcast
10. HUD が pending リスト表示から該当 request を除去、結果ログを短時間表示

### 8. 範囲予告と HUD

Phase 3 の範囲予告は World 空間の DebugSphere 描画と画面左下のテキストで構成する。UMG への移行は Phase 4+。

`ACallsignDebugHUD` を以下のとおり拡張する。

- `BeginPlay` で `UCallsignSupportSystem` を世界からフェッチして `WeakObjectPtr` で保持
- `DrawHUD` 内で `GetPendingRequests` を取得し、各 request について:
  - 画面左下に `[Support N] {Type} {TurnsRemaining}T -> (X,Y,Z)` テキスト (`AHUD::DrawText`)
  - World 空間に `DrawDebugSphere(GetWorld(), request.TargetLocation, request.Definition->RadiusCm, 16, color, false, -1.0f, 0, 2.0f)` を発行
  - `TurnsRemaining == 0` なら色を `FColor::Red`、それ以外は `FColor::Yellow`
- `bShowSupportPreview` (bool, EditDefaultsOnly) を Phase 1 の `bShowTurnInfo` / `bShowLoSPreview` と並列に追加し、PIE デバッグで切替できるようにする

地形破壊半径は同じ位置から別色 (`FColor::Orange`) で `TerrainDestructionRadiusCm` の球を描く。Phase 3 ではプレイヤーが破壊範囲を読めることを優先する。

### 9. 巻き込みと地形破壊

#### 9.1 巻き込み

- `OverlapMultiByObjectType(ECC_Pawn)` で範囲内 Pawn を取得
- 各 Pawn に対し `FCallsignShotRequest` 風の構造でダメージ送信。Combat resolver の `ResolveShot` 経路を流用するか、ダメージ送信専用の薄いヘルパー (`UCallsignCombatResolver::ApplyDirectDamage(AActor* Target, int32 Damage, AActor* Instigator)` 新設) で済ますかは実装時に判断 (§13 OQ-5 参照)
- Phase 3 では `bIsRequester` かどうかは区別しない (友軍誤射 OK)
- `bAllowsFriendlyFire` フィールドは Data Asset に置くが、Phase 3 では分岐しない (常に true 経路を通る)。Phase 4 IFF 拡張で利用する

#### 9.2 地形破壊

- 爆心地から `TerrainDestructionRadiusCm` 以内の `ACallsignNode` のうち `bDestroyable=true` のものを `bIsDestroyed=true` にする
- 破壊された Node を全ての隣接ノードの `Adjacent` 配列から除去 (片方向除去で OK、Phase 4 で双方向ガベージコレクトを正規化する)
- 破壊された Node に乗ろうとする Pawn の処理: Phase 3 では `CallsignNodeMovement` 側で `bIsDestroyed=true` のターゲットを `MoveToNode` で reject する。Pawn 側の個別実装を避け、移動ロジックの単一窓口で扱う (§13 OQ-5 参照)

破壊済みノードのビジュアル更新 (メッシュ非表示など) は Blueprint 側 / `ACallsignNode` 側のレベルで `OnDestroyed` delegate を broadcast し、見た目は Blueprint で対応する。C++ ロジックは flag のみ扱う。

### 10. 命名規約 (Phase 1+2 から継続)

ADR-002 §6 / ADR-003 §10 の規約をそのまま継続する。新規クラス・型は `Callsign` プレフィックスを採用する。

採用ルール:

- 新規ゲームプレイクラス: `Callsign` プレフィックス + UE 型プレフィックス
  - 例: `UCallsignSupportSystem`, `UCallsignSupportResolver`
- POD/USTRUCT: `FCallsignXxx`
  - 例: `FCallsignSupportRequest`, `FCallsignSupportResolution`
- enum: `ECallsignXxx`
  - 例: `ECallsignSupportType` { PrecisionStrike, SupplyPod, OrbitalBarrage }, `ECallsignSupportPhase` { Pending, Resolving, Resolved, Cancelled }
- Data Asset: `UCallsignSupportDefinition : UPrimaryDataAsset`
- ファイル名はクラス名と一致させる (`CallsignSupportSystem.h` 等)
- Phase 1+2 既存型は改名しない

### 11. データ駆動の境界

Phase 3 で扱うデータの配分は以下とする。

- **支援パラメータ**: `UCallsignSupportDefinition` (UPrimaryDataAsset) を 3 個 (PrecisionStrike / SupplyPod / OrbitalBarrage) 作成。Phase 2 の `UCallsignWeaponDefinition` と同様、ランタイム状態は持たず Static
- **支援ランタイム状態**: `FCallsignSupportRequest` POD で管理。`UCallsignSupportSystem` がキューに保持
- **ノード破壊フラグ**: `ACallsignNode` 上の `UPROPERTY(EditAnywhere) bool bDestroyable` (静的) と `UPROPERTY(VisibleAnywhere, Transient) bool bIsDestroyed` (動的) を追加。`FCallsignNodeAttributes` には入れない (instance state のため。共有 USTRUCT は静的属性のみを担う)
- **支援種別の追加**: `ECallsignSupportType` に enum 値を追加するだけで対応可能 (Phase 4+ で煙幕・セントリー投下等を追加する)

DataTable は Phase 3 では新規追加しない。`UCallsignSupportDefinition` の Data Asset 3 個で十分カバーできる。

### 12. Phase 3 で意図的に保留する項目

以下は Phase 3 では扱わない。クラスもプロパティも作らない。

- ニコイチ (`Combine`) — **Phase 4 別 ADR**。ADR-003 §12 で Phase 3 とした記載は本 ADR で訂正
- 分解 (`Disassemble`) — **Phase 4**。ADR-003 §12 訂正
- 拾った武器への操作 (装備・交換・分解・ニコイチ・放置) — **Phase 4**。ADR-003 §12 訂正
- 武器特性継承 (DamageBonus / KnockbackBonus / RangeBonus / DurabilityRecoveryOnly) — **Phase 4**
- 抽出フェーズ、警戒レベル、増援 — Phase 5
- 主目標、ミッション成功/失敗判定 — Phase 5
- BP 断片、戦闘ログ、初期支給候補解放、セーフティゾーン — Phase 6
- ジャミング (`JammerEnemy` による支援妨害、支援不可範囲) — Phase 5
- IFF (友軍誤射回避、`bAllowsFriendlyFire=false` 経路) — Phase 4 以降
- 支援用消費アイテム化 (Beacon の数量管理、初期支給での Beacon 配布) — Phase 4 以降。Phase 3 では「無限に発注できる」ことを許容
- 連続発注のキュー長制限 (連続支援によるバランスブレイク対策) — Phase 4 以降
- 支援サブキュー (1 ターン内に複数支援を Pending にする際の表示順や同時発動時の処理優先度) — Phase 4 以降
- 屋内・天井・通信状況による支援不可制限 (`ADR.md` §11) — Phase 5 (ジャミングと同時に扱う)
- 騒音・警戒レベル上昇 (`ADR.md` §11) — Phase 5
- 範囲予告の UMG/Niagara 化 — Phase 4+
- 双方向 Adjacent ガベージコレクト (破壊ノードを Adjacent から削除する際の整合性検査) — Phase 4

これらを Phase 3 のクラスにフィールドだけ先取りすることも禁止する (例外: `bAllowsFriendlyFire` のみ Phase 4 IFF 拡張のため Data Asset 上に枠を確保するが、Phase 3 では参照しない)。

### 13. Open Questions

Phase 3 実装中に検証・確定が必要な点。**確定済み判断は本セクションに置かない** (確定済みは §Decision に記載、Phase 4 で覆る場合の移行コストは §Consequences/Risks に記載する)。

1. `ACallsignNode::Adjacent` の双方向クリーンアップを Phase 3 で実装するか、`bIsDestroyed=true` のチェックだけで Phase 4 まで誤魔化すか。Phase 3 では片方向除去の暫定実装を採用するが、移動ロジックで `bIsDestroyed=true` ノードを reject する追加防御が必要 (§9.2)
2. 範囲予告を World 空間 `DrawDebugSphere` で済ませる方針 (§8) は Phase 3 完了条件 (b)(c) を満たすかプレイテストで判定する。プレイヤーが範囲を読めない場合は UMG 早期移行を Phase 4 で前倒しする
3. 支援が複数同時 pending のとき、HUD/ログ上の表示順序ルール (FIFO / `TurnsRemaining` 昇順 / 種別順)。Phase 3 では `RequestId` 生成順 (FIFO) を仮採用するが、プレイテストで判断
4. SupplyPod が「着弾地点にダメージ」を実装するか (PLAN.md は明記)。`bDealsDamage` Data Asset フラグで切替可能にする方針を採用するが、Phase 3 完了時点でデフォルト値を true にするか false にするかを確定する
5. 破壊された Node に乗ろうとする `MoveToNode` の reject ロジックを `CallsignNodeMovement` (一元化) に入れるか、Pawn 側で個別に入れるか。§9.2 では `CallsignNodeMovement` 側採用案を提示するが、Phase 3 で実装するときに既存 `Pawns/` 構造との衝突がないか確認する
6. `UCallsignSupportSystem` を `UWorldSubsystem` にするか `UGameInstanceSubsystem` にするか。Phase 3 では PIE 単一マップ前提で前者を採用するが、Phase 5 で作戦マップとアリーナを分けたとき pending 支援がマップ越境するかを別途確定する

## Consequences

### Positive

- Phase 1+2 の単一モジュール構成・サブディレクトリ分割・命名規約をそのまま維持し、`Support/` 新設のみで局所追加できる
- Combat / LoS / Turn を流用するため、支援固有の実装量を抑えられる (`Support/` 配下の C++ クラスは 2 個 + Data Asset 3 個 + 既存型への拡張で済む)
- 支援キューと Turn 進行を結合することで、Phase 3 完了条件 (c) 「支援待ちのターンが面白い」を達成しやすい (Pending リストが時間経過を可視化する)
- Data Asset 駆動 (`UCallsignSupportDefinition`) により、`DelayTurns` / `RadiusCm` / `Damage` 等のチューニングが C++ 再ビルド不要で行える
- Phase 1 の `ACallsignDebugHUD` を流用するため、Phase 3 PIE 検証が即時可能 (UMG 設計を後回しにできる)

### Negative

- ノード破壊で `Adjacent` グラフに穴が空く管理が新規発生する。Phase 3 では片方向除去の暫定実装で済ませるが、Phase 4 で双方向ガベージコレクトの正規化作業が必要
- `DrawDebugSphere` ベースの範囲予告は、Phase 4 で UMG / Niagara 化するときに置換コストが発生する
- Phase 3 では IFF を扱わないため、「友軍誤射の判断」がプレイテストで荒れる可能性がある (Phase 3 完了条件 (b)「自爆や地形破壊が納得できる」は「巻き込みを織り込んで判断する」前提で評価する)
- `UCallsignSupportSystem` と `UCallsignSupportResolver` の責務分離は、Phase 3 の支援 3 種規模では過剰に見える可能性がある。ただし Phase 4 ジャミング拡張時に System 側だけを変更する余地を残す目的で分離を維持する (§Risks)
- 支援が無限発注可能 (`Beacon` 残量管理なし) であるため、プレイテストで「乱発で壊れる」可能性。Phase 3 完了条件 (a)「支援要請が強いが雑に使えない」は遅延ターンと巻き込みリスクで支えるが、Phase 4 で消費アイテム化を検討する

### Risks

- 範囲予告の精度がプレイヤーの判断を支えるため、`DrawDebugSphere` の見た目が荒すぎるとゲームが成立しない (Phase 3 完了条件 (b)(c) のリスク)。プレイテスト初期で UMG 化判断を早める可能性がある
- 地形破壊が Adjacent に反映されないと、Pawn が空中ノード (孤立した `bIsDestroyed=true` 隣接ノード) へ移動できてしまうバグが起きる。`CallsignNodeMovement` 側での reject ロジック (§9.2) と Adjacent 片方向除去の両方を実装で漏らさないこと
- 遅延発動の `TurnsRemaining` 0 計算で off-by-one が起きやすい。`OnTurnEnd` で `--TurnsRemaining` 後に `<= 0` 判定を行うか、`-- 後 == 0` 判定にするかを実装時に明文化する。Phase 3 完了時の Test Plan に以下を含める:
  - Unit Test: `DelayTurns=2` の支援を提出し、提出ターン後 2 回の turn end で発動する (1 回目では発動しない)
  - Unit Test: `DelayTurns=1` の支援を提出し、提出ターン後 1 回の turn end で発動する
  - Unit Test: 同 turn end に複数 request の `TurnsRemaining` が 0 になったとき、全てが Resolve される
- 巻き込み判定の `OverlapMultiByObjectType` が高 Pawn 数で重い可能性 (Phase 3 では n=数体なので問題化しにくいが、Phase 5 増援で n が増える)。Phase 5 で Profile し直す
- Phase 5 ジャミング (支援妨害) を見越した拡張点を `UCallsignSupportSystem` に確保しておかないと、Phase 5 で大規模リファクタになる。具体的には `RequestSupport` 内で「ジャマー存在判定 → 拒否 or 遅延延長」フックを追加できる構造を Phase 3 設計時点で意識する (Phase 3 ではフック実装は不要だが、`SubmitRequest` の入口がボトルネックになることを確認する)
- `bAllowsFriendlyFire` フィールドを Data Asset に Phase 3 で先取りするのは、ADR-002 §8 / ADR-003 §12 の「先取り禁止」原則に対する例外である。Phase 4 IFF 拡張で型変更を回避するための判断として例外を許容するが、Phase 3 では値を参照しないこと。実装中に参照経路が紛れ込んだら ADR 違反として差し戻す
- `UCallsignSupportResolver` を Phase 3 で UWorldSubsystem 化したが、Phase 4 で支援の同時発動が増えると Resolver の再入処理 (Resolve 中に別 Resolve を呼び出す) でロックが必要になる可能性がある。Phase 3 では同 turn 内の複数発動を直列処理で済ませるが、Phase 4 設計時に再点検する
