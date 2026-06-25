# JojiBaseballEngine — インフラ実装ロードマップ

## 最終ゴール

- **シーズン stats** を Vercel サイトに表示する
- **夜 9 時に固定シード試合**をリアルタイム配信する (球筋・打球・スコアが流れる)
- URL: https://jojistats-app-jojicoders-projects.vercel.app/stats

---

## アーキテクチャ決定事項

| 項目 | 選択 | 理由 |
|------|------|------|
| フロントエンド | Vercel (既存 React サイト) | すでに動いている |
| バックエンド | Oracle Cloud Always Free ARM VM | 永続無料、C++ が動く |
| リアルタイム通信 | WebSocket | 球ごとのプッシュに最適 |
| シード固定 | `Random{seed: 20260900 + dayOfYear}` | 全ユーザーが同じ試合を同時視聴 |
| WASM | 不採用 | デバッグ困難、モバイル負荷大、シードずれリスク |

---

## フェーズ 1 — シーズン stats 表示

### やること

1. **CI/GitHub Actions で JSON を自動生成**
   - `JojiSeasonRunner 100 --json > public/stats.json` を定期実行 (週 1 など)
   - または手動で生成した `stats.json` を Vercel の `public/` に置く

2. **Vercel React 側で stats.json を fetch して表示**
   - `/stats` ページに以下を表示:
     - チーム順位表 (W/L/PCT/RS-RA)
     - 打撃リーダー (AVG/OBP/SLG/OPS/wOBA/HR/SB)
     - 投手リーダー (ERA/WHIP/K9/BB9/SV)
   - データ取得: `fetch('/stats.json')` でシンプルに

3. **デプロイ確認**

### JSON フォーマット (すでに実装済み)

```bash
./JojiSeasonRunner 100 --json > stats.json
```

```json
{
  "seasons": 100,
  "teams": [
    { "name": "Brooklyn Hammers", "wins": 58.4, "losses": 37.6, "pct": 0.609, "rsPerGame": 4.99, "raPerGame": 3.76 }
  ],
  "battingLeaders": [
    { "name": "Joji Rivera", "team": "Newark Knights", "pa": 96000, "avg": 0.297, "obp": 0.371, "slg": 0.452, "ops": 0.823, "woba": 0.371, "hr": 1559, "sb": 1559 }
  ],
  "pitchingLeaders": [
    { "name": "Jake Ford", "team": "Fishtown Ferals", "gs": 1900, "gr": 0, "ip": 10841.0, "era": 3.08, "whip": 1.23, "k9": 9.1, "bb9": 2.7, "sv": 0 }
  ]
}
```

### 作業量: 小 (半日〜1日)

---

## フェーズ 2 — リアルタイム試合配信

### アーキテクチャ

```
[Oracle Cloud VM]                    [Vercel / ブラウザ]
 C++ GameEngine
   ↓ 1球ずつ simulateNextPitch()
 WebSocket Server (C++ / uWebSockets)  ←→  React クライアント
   ↓ JSON push per pitch              球筋アニメーション
 毎日 21:00 JST に固定シードで起動         スコアボード更新
```

### やること (Oracle Cloud VM 側)

1. **Oracle Cloud Always Free VM 作成**
   - ARM Ampere A1 (4 OCPU / 24GB RAM) — 永続無料
   - Ubuntu 22.04
   - ポート 8080 (WebSocket) を Security List で開放

2. **C++ WebSocket サーバー構築**
   - ライブラリ: `uWebSockets` または `libwebsockets` (apt で入る)
   - 毎日 21:00 JST に `GameEngine{seed: date_seed}` を起動
   - `simulateNextPitch()` を 300ms ごとに呼び、結果を JSON で全クライアントに broadcast
   - systemd サービスとして常駐

3. **送信 JSON フォーマット (1球ごと)**

```json
{
  "type": "pitch",
  "inning": 1,
  "isTop": true,
  "count": { "balls": 1, "strikes": 0 },
  "outs": 0,
  "score": { "away": 0, "home": 0 },
  "pitcher": "Jake Ford",
  "batter": "Joji Rivera",
  "pitch": {
    "type": "Fastball",
    "velocity": 94.2,
    "outcome": "Ball"
  },
  "trajectory": [[0,0,0],[1.2,8.4,2.1],...],
  "bases": { "first": null, "second": "Tomas Ruiz", "third": null }
}
```

### やること (Vercel React 側)

1. **WebSocket 接続**
   ```js
   const ws = new WebSocket('wss://your-oracle-vm-ip:8080')
   ws.onmessage = (e) => updateGame(JSON.parse(e.data))
   ```

2. **リアルタイム表示コンポーネント**
   - スコアボード (イニング別得点)
   - 打席情報 (投手/打者/カウント)
   - 球筋アニメーション (Three.js または Canvas 2D)
   - 塁上走者表示

3. **試合前 (21:00 前) の表示**
   - 先発投手・打順を事前表示
   - カウントダウン

### 作業量: 大 (1〜2週間)

---

## 実装順序

```
[ 今ここ ]
    ↓
フェーズ 1: stats.json を Vercel に置いて表示 (半日)
    ↓
フェーズ 2a: Oracle Cloud VM セットアップ + C++ WebSocket サーバー
    ↓
フェーズ 2b: React WebSocket クライアント + スコアボード
    ↓
フェーズ 2c: 球筋アニメーション
    ↓
[ 完成 ]
```

---

## まず最初にやること

**フェーズ 1 の Step 1:**

```bash
# エンジンをビルドしてシーズン stats を生成
cd /Users/jojo/Desktop/JojiBaseballEngine/app
make season
./build/JojiSeasonRunner 100 --json > stats.json
```

→ この `stats.json` を Vercel プロジェクトの `public/` フォルダに置く  
→ React の `/stats` ページで `fetch('/stats.json')` して表示

**Vercel プロジェクトは `jojistats-app-jojicoders-projects` (既存)**

---

## メモ

- Oracle Cloud 登録: https://cloud.oracle.com (クレカ必要だが課金なし)
- uWebSockets: https://github.com/uNetworking/uWebSockets
- 球筋データは `AnimationPlan` に入っている (`latestAnimationPlan()` で取得)
- シード計算例: `seed = 20260000 + year*10000 + month*100 + day`
