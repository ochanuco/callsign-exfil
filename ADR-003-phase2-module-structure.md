# ADR-003: Phase 2 (Weapon Resource Prototype) モジュール構造

## Status

Accepted

## Context

`PLAN.md` の Phase 2 (Weapon Resource Prototype) では、武器3種・弾薬2〜3種・マガジン・リロード・タクティカルリロード・耐久減少・破損・救済ハンドガンを実装し、リロード判断が発生すること、武器破損が理不尽でないこと、救済ハンドガンが詰み防止として機能することを完了条件とする。Phase 1 で確立した `CallsignExfil` 単一モジュール + サブディレクトリ分割の上に、武器リソース管理層を追加で乗せる。Phase 1 段階の `UCallsignWeaponDefinition` は `Damage` / `Range` / `bRequiresLineOfSight` のみ、`UCallsignCombatResolver` は単発・耐久なし・弾薬なしの仮実装であり、Phase 2 ではこれらを実体化させ、武器リソース管理がゲーム性として成立するかを検証する。本 ADR は Phase 2 範囲に限定し、Phase 3 以降のニコイチ・分解・武器特性・拾得操作・支援要請・抽出フェーズには踏み込まない。

## Decision

### 1. Phase 2 スコープ確認

Phase 2 で実装するのは以下に限る。

- 武器3種 (Assault Rifle / Shotgun / Launcher。Rescue Handgun は Phase 1 から継承して Phase 2 仕様に拡張)
- 弾薬3種 (LightAmmo / ShellAmmo / HeavyAmmo)
- マガジン (装填弾数、残弾)
- 通常リロード
- タクティカルリロード (残弾破棄)
- 耐久減少 (射撃行動ごと)
- 破損 (耐久 0 で射撃不可)
- 救済ハンドガン特例 (耐久減少なし、弾薬無限)

完了条件は `PLAN.md` の Phase 2 セクションに従う。

### 2. モジュール分割 (Phase 1 構造への拡張)

Phase 1 で採用した単一モジュール `CallsignExfil` 構成は維持する。複数モジュール化は Phase 2 でも行わない。サブディレクトリで論理サブシステムを分離する方針も継続する。

`game/CallsignExfil/Source/CallsignExfil/` 配下に Phase 2 で以下を追加する。

- `Inventory/` 新設 — 武器スロット保持、弾薬プール、リロード操作の入口
- `Weapon/` 新設 — 武器インスタンス (耐久・マガジン残弾を持つランタイム状態)、破損判定
- `Combat/` (既存) — Phase 2 で耐久消費・弾薬消費・破損チェックを行うように拡張
- `Data/` (既存) — `UCallsignWeaponDefinition` の拡張、`ECallsignAmmoType` 等の追加

`Weapon/` を新設するか、`Data/` 内に格納するかを検討したが、`UCallsignWeaponDefinition` (静的データ) と `UCallsignWeaponInstanceObject` (ランタイム状態) の責務が明確に異なるため、`Data/` には静的データのみ置き、ランタイム状態は `Weapon/` に分離する方針を採用する。

各サブシステムの owned concern は次のとおり。

| サブシステム | Owned concern                                                                                                |
| ------------ | ------------------------------------------------------------------------------------------------------------ |
| Inventory    | Pawn が保持する武器スロット、装備中武器の追跡、弾薬プール残量、リロード操作の窓口                            |
| Weapon       | 武器インスタンスの状態 (耐久・マガジン残弾・摩耗)、破損判定、耐久消費、弾薬消費の内部処理                    |
| Combat       | 射撃要求の解決順序 (弾薬チェック → マガジン消費 → 耐久消費 → 破損判定 → LoS+ダメージ適用)                    |
| Data         | `UCallsignWeaponDefinition` の Phase 2 拡張、弾薬種・スロット種の enum、リロード結果 USTRUCT                 |

### 3. データ層の拡張

#### 3.1 UCallsignWeaponDefinition の拡張

Phase 1 の `UCallsignWeaponDefinition` (Damage / Range / bRequiresLineOfSight) はそのまま維持し、Phase 2 で以下の `UPROPERTY` を追加する。並行リネームや型分割は行わない。

- `AmmoType` (`ECallsignAmmoType`) — 消費する弾薬種
- `bUsesAmmoPool` (bool) — 弾薬プールから消費するか。救済ハンドガンは false
- `MagazineSize` (int32) — マガジン装填弾数
- `ShotsPerAction` (int32) — 1射撃行動で発射される弾数 (Handgun=1 / AR=3 / Shotgun=1 / Launcher=1)
- `DurabilityMax` (int32) — 最大耐久
- `DurabilityCostPerAction` (int32) — 1射撃行動あたりの耐久減少量
- `bIsRescue` (bool) — 救済ハンドガン特例。true なら耐久減少なし・弾薬プール非参照
- `WeaponSlot` (`ECallsignWeaponSlot`) — 装備スロット

#### 3.2 新規 USTRUCT / enum

以下を `Data/CallsignTypes.h` または分割ヘッダ (`Data/CallsignWeaponTypes.h` 新設) に追加する。Phase 1 既存型は変更しない。

- `ECallsignAmmoType` — `Light` / `Shell` / `Heavy`
- `ECallsignWeaponSlot` — `Main` / `Secondary` / `Power` / `Rescue`
- `FCallsignWeaponInstance` USTRUCT — `WeaponDef` (TObjectPtr<UCallsignWeaponDefinition>) / `MagazineCurrent` (int32) / `DurabilityCurrent` (int32) / `WearLevel` (int32)
- `FCallsignAmmoPool` USTRUCT — `AmmoType` (ECallsignAmmoType) / `Count` (int32)
- `FCallsignReloadResult` USTRUCT — `bReloaded` / `bWasTactical` / `DiscardedRounds` / `LoadedRounds`
- `FCallsignShotRequest` への追記: `Inventory` (TWeakObjectPtr<UCallsignInventoryComponent>) を追加。`Weapon` (`UCallsignWeaponDefinition*`) は保持し、ランタイム消費は Inventory 経由で行う

`FCallsignShotRequest` の Phase 1 シグネチャは破壊しない。Inventory 参照は新規フィールドとして追加し、未指定時は Phase 1 互換 (耐久・弾薬を消費しない) で動作する。

### 4. クラス分担スケッチ

UE プレフィックス規約 (Phase 1 ADR-002 §6 と同一) に従う。Phase 2 で導入する主要クラスは以下とする。

#### 4.1 Inventory

> `UCallsignInventoryComponent : UActorComponent`: Pawn が保持するインベントリ。武器スロット (`Main` / `Secondary` / `Power` / `Rescue`) と弾薬プール (`TMap<ECallsignAmmoType, int32>`) を持つ。Phase 2 では Pawn コンストラクタで自動付与する。
> 公開 API (BlueprintCallable): `EquipWeapon(ECallsignWeaponSlot, UCallsignWeaponInstanceObject*)` / `GetCurrentWeapon() const` / `ConsumeShot(FCallsignShotResult& OutResult)` / `Reload()` (通常もタクティカルも内部で振り分け) / `GetAmmoCount(ECallsignAmmoType) const` / `IsReloadAvailable() const`

#### 4.2 Weapon

> `UCallsignWeaponInstanceObject : UObject`: 武器インスタンスの実体。`FCallsignWeaponInstance` を内部状態として保持し、振る舞いをメソッドとして公開する。`UObject` ベースを採用する理由は、(a) BlueprintCallable で `GetMagazineCurrent` / `GetDurabilityCurrent` / `IsBroken` を露出しやすい、(b) Phase 3 のニコイチで状態遷移が複雑化したときに `UPROPERTY` 拡張が容易、(c) UI 連携が `TObjectPtr` で扱える。GC コストは Pawn ごとに数個のため許容する。
> 公開 API (BlueprintCallable): `GetMagazineCurrent() const` / `GetDurabilityCurrent() const` / `IsBroken() const` / `ApplyDurabilityCost()` / `ConsumeMagazineRound(int32 Count)` / `ReloadFromPool(int32 Available, FCallsignReloadResult& OutResult)`

POD `FCallsignWeaponInstance` をそのまま公開する案も検討したが、Phase 3 のニコイチで耐久回復・最大耐久低下・特性継承を扱う際に状態機械化が必要となるため、Phase 2 段階で UObject 化する判断を確定させる。

#### 4.3 Combat (既存拡張)

> `UCallsignCombatResolver` の `ResolveShot` を Phase 2 では以下の順序で処理する。`FCallsignShotRequest` に Inventory 参照が含まれる場合のみ Phase 2 経路を通り、未指定なら Phase 1 互換経路を維持する。
>
> 1. Inventory から現在武器を取得
> 2. 弾薬チェック (`bUsesAmmoPool` のとき): プールに弾なしなら Hit=false で即時 return
> 3. マガジン残弾チェック: 0 なら Hit=false で即時 return (UI 側で「リロードしろ」を表示)
> 4. 破損チェック: `IsBroken()` なら Hit=false で即時 return
> 5. マガジン消費 (`ShotsPerAction` 分)、必要なら弾薬プール消費
> 6. 耐久消費 (`bIsRescue=false` のときのみ `DurabilityCostPerAction` を適用)
> 7. 破損判定 (`DurabilityCurrent <= 0` で `IsBroken=true` を確定、`OnWeaponBroken` を broadcast)
> 8. 既存の LoS+ダメージ適用 (Phase 1 の処理を継承)

### 5. C++ vs Blueprint 配分ポリシー

Phase 1 の方針 (ADR-002 §4) を踏襲する。Phase 2 で追加する責務の配分は以下とする。

- C++: 弾薬チェック、マガジン消費、耐久計算、破損判定、リロード種別判定 (通常/タクティカル)、救済特例分岐
- Data Asset (Editor): 武器パラメータ (Damage / Range / MagazineSize / ShotsPerAction / DurabilityMax / DurabilityCostPerAction / AmmoType / bUsesAmmoPool / bIsRescue) を `UCallsignWeaponDefinition` インスタンスとして 4 種 (Handgun / AR / Shotgun / Launcher) 用意する
- Blueprint / UMG: リロードアニメーション、破損 SE/VFX、マガジン残弾と耐久バーの表示
- 救済ハンドガン特例 (`bIsRescue`) は C++ ロジックの分岐で扱い、Blueprint 側で個別処理を追加させない

判定ロジックが Blueprint へ漏れるとリソース管理ルールの検証が困難になるため、消費・減少・破損の 3 系統は必ず C++ で完結させる。

### 6. 依存方向

Phase 1 の DAG (ADR-002 §5) を維持しつつ、`Inventory` / `Weapon` を追加する。循環依存は禁止する。

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
        |          +-----------+
        |          |   Node    |
        |          +-----+-----+
        |                |
        v                v
   +---------+   +---------------+
   | Combat  |-->|   LineOfSight |
   +----+----+   +-------+-------+
        |               |
        v               |
   +-----------+        |
   | Inventory |        |
   +-----+-----+        |
         |              |
         v              v
   +---------+    +-----------+
   | Weapon  |--->|   Data    |
   +---------+    +-----------+
```

ルール:

- `Data` は他に依存しない (純粋な型定義)。
- `Weapon` は `Data` のみ参照する。
- `Inventory` は `Weapon` と `Data` を参照する。Pawn 具象型は知らない。
- `Combat` は `Inventory` / `Weapon` / `Data` / `LineOfSight` を参照する (Phase 1 から `Inventory` / `Weapon` 依存を追加)。
- `Pawns` は `Inventory` をコンポーネントとして所有する (`UActorComponent` 経由のため `Pawns → Inventory` の依存は許容)。
- `Turn` は引き続き `ICallsignTurnParticipant` 越しにのみ参加者を知る。リロードもターン消費アクションのため Turn は Inventory を直接知らず、`PlayerController` 経由でターンを終端する。
- `Core` のみが上記すべてを束ねる。

### 7. リロード仕様の実装方針

`ADR.md` §8 を C++ にどう写すかを以下に定める。

- `ACallsignExfilPlayerController::Reload()` (新規 BlueprintCallable): `IsMyTurn` を確認し、Inventory の `Reload()` を呼び、結果を受けてターン消費 (Turn サブシステムへ `EndTurn` 通知) を行う。
- `UCallsignInventoryComponent::Reload()` の内部分岐:
  - 現在武器が `bIsRescue=true` の場合は no-op (リロード不要)
  - `MagazineCurrent > 0` なら タクティカルリロード — マガジン残弾を破棄、所持弾薬から `MagazineSize` まで装填
  - `MagazineCurrent == 0` なら 通常リロード — 所持弾薬から `MagazineSize` まで装填
  - 所持弾薬が `MagazineSize` 未満なら、所持弾薬全量を装填
- 耐久減少は発生しない (`ADR.md` §8 明記)。
- UI 通知: `UCallsignInventoryComponent` に delegate `OnReloaded(FCallsignReloadResult)` を追加し、PlayerController と UMG が購読する。

### 8. 耐久・破損の実装方針

`PLAN.md` §Weapon Durability MVP Durability Model に従い、「射撃行動ごとの耐久減少」を Phase 2 では暫定採用する。発砲弾数連動 (1発ごとに減少) は採用しない。

- 1射撃行動あたりの減少量 = `DurabilityCostPerAction` (3点バースト AR でも 1 行動 = 1 回の減少)
- `bIsRescue=true` の武器は減少なし
- `DurabilityCurrent <= 0` で `IsBroken() = true`、以降の射撃要求は Combat 側で即時拒否
- 破損確定時に `UCallsignWeaponInstanceObject` の delegate `OnWeaponBroken(UCallsignWeaponInstanceObject*)` を broadcast し、Inventory・PlayerController・UMG が購読する
- `PLAN.md` 既定値: AR -2 / Shotgun -1 / Launcher -2 / Handgun 0 を Data Asset に直書きし、プレイテストで調整する

### 9. 救済ハンドガンの扱い

Pawn の Inventory 構築時に必ず `Rescue` スロットへ装着する。「無限」を `TNumericLimits<int32>::Max()` のような巨大値で表現するのは UI でのパーセンテージ計算や残量加算で算術オーバーフローを招くため避ける。代わりに **明示フラグ** で無限を表現し、算術処理の前段で必ず分岐する。

Data Asset としての特例パラメータは以下とする。

- `bIsRescue = true`
- `bHasInfiniteDurability = true`
- `bHasInfiniteMagazine = true`
- `DurabilityCostPerAction = 0`
- `DurabilityMax = 1` (フラグ true の場合は数値自体を参照しないが、デフォルト 0 でゼロ除算を踏まないよう正の値を入れておく)
- `MagazineSize = 1` (同上)
- `bUsesAmmoPool = false` (`AmmoType=Light` のままだが弾薬プールを参照しない)
- `WeaponSlot = Rescue`

実装上のルール:

- `UCallsignWeaponInstanceObject::IsBroken()`: `bHasInfiniteDurability` true なら常に false を返す。`DurabilityCurrent` の演算より先に分岐する。
- `UCallsignWeaponInstanceObject::ConsumeShot()` / `ApplyDurabilityCost()`: 同 flag true なら現在値を変えない。
- `UCallsignInventoryComponent::Reload()`: `bHasInfiniteMagazine` true なら no-op で即時 return。タクティカル / ノーマルの分岐に入る前にチェックする。
- UI 側の残量パーセンテージ表示は flag を見て「∞」または非表示に切り替える。`100 * Current / Max` の演算は flag false のときだけ通す。

設計簡略化のため、Phase 2 では Light 弾薬プールを救済ハンドガンと AR で共有しないハードコード扱いを許容する。`bUsesAmmoPool=false` の分岐を Inventory・Combat 双方で素直に書く。

### 10. 命名規約 (Phase 1 から継続)

ADR-002 §6 の規約をそのまま継続する。新規クラス・型は `Callsign` プレフィックスを採用する。

採用ルール:

- 新規ゲームプレイクラス: `Callsign` プレフィックス + UE 型プレフィックス
  - 例: `UCallsignInventoryComponent`, `UCallsignWeaponInstanceObject`
- POD/USTRUCT: `FCallsignXxx`
  - 例: `FCallsignWeaponInstance`, `FCallsignAmmoPool`, `FCallsignReloadResult`
- enum: `ECallsignXxx`
  - 例: `ECallsignAmmoType` { Light, Shell, Heavy }, `ECallsignWeaponSlot` { Main, Secondary, Power, Rescue }
- ファイル名はクラス名と一致させる (`CallsignInventoryComponent.h` 等)
- Phase 1 既存型 (`UCallsignWeaponDefinition`, `FCallsignShotRequest`, `FCallsignShotResult`, `UCallsignCombatResolver`) は改名しない。Phase 2 拡張は同一型に追加する形で行う

### 11. データ駆動の境界

Phase 2 で扱うデータの配分は以下とする。

- **武器ステータス**: `UCallsignWeaponDefinition` (UPrimaryDataAsset) の 1 個 = 1 武器種。Phase 2 では Handgun / AR / Shotgun / Launcher の 4 個の Data Asset を作成する。Phase 1 の単一武器 Data Asset は AR 用に流用してもよいし、Handgun 用に振り直してもよい (運用判断)
- **弾薬種**: `ECallsignAmmoType` enum で固定 (Light / Shell / Heavy)。DataTable 化はしない
- **弾薬プール残量**: `FCallsignAmmoPool` USTRUCT を Inventory に `TMap<ECallsignAmmoType, int32>` で持たせるランタイム状態 (永続化なし)
- **マガジン残弾・現在耐久**: `UCallsignWeaponInstanceObject` のランタイム状態。Data Asset には乗せない

DataTable は Phase 2 では新規追加しない。`FCallsignEnemyStats` 用の DataTable (Phase 1) はそのまま運用する。

### 12. Phase 2 で意図的に保留する項目

以下は Phase 2 では扱わない。クラスもプロパティも作らない。

- ニコイチ (`Combine`) — Phase 3 で別 ADR 化
- 分解 (`Disassemble`) — Phase 3
- 武器特性継承 (DamageBonus / KnockbackBonus / RangeBonus / DurabilityRecoveryOnly) — Phase 3 ニコイチで初登場
- 拾った武器への操作 (装備・交換・分解・ニコイチ・放置) — Phase 3
- 支援要請、遅延発動、地形破壊 — Phase 3
- 抽出フェーズ、警戒レベル、増援 — Phase 4
- 弾薬種の追加 (`PowerAmmo` 等) — Phase 2 では Light / Shell / Heavy のみ
- 状態異常、追加効果 (ノックバック以外の二次効果) — Phase 4 以降
- 武器持ち替えのターンコスト確定 — `Deferred Decisions` のまま
- 摩耗レベル (`WearLevel`) の挙動 — Phase 2 では `FCallsignWeaponInstance` に枠だけ用意し、増減ロジックは未実装

これらを Phase 2 のクラスにフィールドだけ先取りすることも禁止する (例: `bIsCombineMaterial` を Phase 2 で追加してはならない)。

### 13. Open Questions

Phase 2 実装中に検証・確定が必要な点:

1. `UCallsignWeaponInstanceObject` を UObject 化する判断は本 ADR で確定したが、実装後に GC コストが Pawn インベントリ規模で問題化しないか計測する必要がある。問題化したら POD `FCallsignWeaponInstance` を直接 Inventory が保持する形へ後退させる選択肢を残す。
2. `UCallsignInventoryComponent` を `UActorComponent` として Pawn に持たせるか、`PlayerController` の `UPROPERTY` として持たせるか。Pawn 切替 (Phase 3 以降の死亡・捕虜想定) で引き継ぎが必要なら後者だが、Phase 2 段階では前者で進める。
3. 弾薬プールを Pawn ごとに持たせるか、PlayerController 越しの「作戦持ち込み弾薬」として持たせるか。敵 Pawn にも弾薬概念を持たせるかで決まるが、Phase 2 では敵側の弾薬は無限とし、プレイヤー Pawn ごとに持たせる方針で進める。
4. 救済ハンドガンを別スロット (`Rescue`) として enum に持つ方針は採用したが、UI のスロット表示で 5 枠並べるか 4+常駐表記にするかは UMG 設計時に決める。
5. リロード/破損のアニメ通知 (delegate broadcast の購読側 UMG/SE) を Phase 2 で実装するか、Phase 3 まで TODO に残すか。Phase 2 完了条件 (`PLAN.md`) はゲームプレイ判断が成立すれば足りるため、最低限のテキストログ/HUD 通知に留めて Phase 3 で見栄えを整える方針を提案する。

## Consequences

### Positive

- Phase 1 の単一モジュール構成・サブディレクトリ分割・命名規約を維持したまま、武器リソース層を局所追加できる。
- `UCallsignWeaponDefinition` を破壊せず拡張する方針により、Phase 1 で作成済みの Data Asset と Combat 経路を Phase 2 でもそのまま使える。
- Inventory / Weapon / Combat の責務が分離されているため、Phase 3 でニコイチ・分解を追加する際に Inventory に局所変更で済ませやすい。
- 救済ハンドガン特例を `bIsRescue` フラグ + `bUsesAmmoPool=false` で吸収することで、Rescue 専用クラスを作らずに済む。
- 「射撃行動ごとの耐久減少」を Data Asset 駆動にしたため、プレイテストでの調整が C++ 再ビルド不要で行える。

### Negative

- `FCallsignShotRequest` に Inventory 参照を追加するため、Phase 1 経路と Phase 2 経路の二系統が `UCallsignCombatResolver::ResolveShot` 内に共存する。Phase 2 完了時点で Phase 1 経路を撤去する判断を別途行う必要がある。
- `UCallsignWeaponInstanceObject` を UObject 化したため、Pawn ごとに 4 個 (スロット数) + 弾倉/耐久状態の UObject を抱えることになり、敵 Pawn にも将来同等構造を入れると GC コストが累積する可能性がある。
- 武器パラメータが `UCallsignWeaponDefinition` 1 ヘッダに集約されるため、Phase 3 で武器特性 (`DamageBonus` 等) を足すと UPROPERTY が肥大化する。Phase 3 で定義分割を検討する必要がある。
- 弾薬プールと武器インスタンスが別オブジェクト責務になったため、UI 側で 2 箇所を購読する必要が生じる。

### Risks

- `DurabilityCostPerAction` の数値感度がプレイテストで合わず、PLAN.md の暫定値 (AR -2 / Shotgun -1 / Launcher -2) を Data Asset 経由で頻繁に書き換える運用になる可能性がある。書き換え自体は容易だが、本 ADR の §8 採用判断 (行動単位減少) を見直す可能性が残る。
- 救済ハンドガンの `bIsRescue=true` 経路が Combat / Inventory 双方で分岐するため、片方で実装漏れが起きると「弾薬プールを Light で消費してしまう」「破損する」等の事故が起きやすい。テストで網羅する必要がある。
- `UCallsignInventoryComponent` を Pawn 所有にしたため、Pawn 切替時 (死亡 → リスポーン or 別 Pawn 操作) のインベントリ移譲を Phase 3 で別途設計する必要が生じる。
- リロード・破損の delegate 設計が Phase 2 段階で粗いまま固まると、Phase 3 のニコイチ通知 (耐久回復イベント等) を後付けで上書きする際に整合が取りにくくなる。Phase 3 設計時に delegate 体系を再点検する。
- Phase 1 の `FCallsignShotRequest` ベース API を残したまま Inventory 経路を追加するため、外部呼び出し側 (Blueprint / Test / 敵 AI) が Phase 1 経路を呼び続け、Phase 2 のリソース消費が反映されない事故が起きうる。Phase 2 完了時に Phase 1 経路の deprecation を明示する必要がある。
