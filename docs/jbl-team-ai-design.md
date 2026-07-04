# JBL Team AI 設計仕様

コードなし、設計仕様として読める形にまとめたもの。

## 基本方針

チームAIは、各球団の個性を実際の行動に反映するための仕組み。

各チームは次の3つで動く。

1. チーム哲学
2. 現在のチーム状態
3. チームスタイル

---

## 1. チーム哲学

チーム哲学は、球団がどんな選手を好み、どんな勝ち方を目指すかを決める。

### Win Now

今すぐ勝つことを最優先する。

- 即戦力を重視
- 高額FAにも積極的
- 若手を出してでも補強する
- 優勝争い中は強気に動く

対象例：
- Bronx Wolves
- Georgetown Ravens

### Balanced Baseball

大きな偏りを作らず、投打のバランスを重視する。

- 穴を埋める補強
- 極端な賭けは少ない
- 先発、打線、守備を平均以上に保つ
- 長期的に安定した成績を目指す

対象例：
- Queens Titans
- Germantown Colonials
- Capitol Hill Senators
- Alexandria Cannons

### Player Development

若手育成を中心にチームを作る。

- 高ポテンシャルの若手を重視
- ドラフトを重要視
- 高額ベテランは避ける
- 数年後の強さを狙う

対象例：
- Staten Island Foxes
- South Philly Stallions

### Power Baseball

長打力と球威を重視する。

- powerの高い打者を好む
- 三振が多くても長打力を評価
- 速球派投手や強肩選手を好む
- バントや細かい野球は少なめ

対象例：
- Brooklyn Hammers
- Fishtown Ferals
- Anacostia Kings

### Small Ball

出塁、走塁、コンタクト、状況判断を重視する。

- contact、eye、speedを高評価
- 盗塁、バント、ヒットエンドランを使う
- 長打力よりもつなぐ力を重視
- 守備や走塁も評価する

対象例：
- Harlem Eagles
- Manayunk Runners
- Newark Knights

### Defense First

守備力と安定感を重視する。

- fielding、arm、catcher、SS、CFを評価
- 失点を減らす編成を好む
- 守備固めを積極的に使う
- 派手な攻撃力より崩れにくさを重視

対象例：
- Kensington Iron

### Pitching First

投手力を最優先する。

- 先発投手を最重要視
- リリーフや守備型捕手も高評価
- ロースコアの試合を好む
- 打線より失点を防ぐことを重視

対象例：
- Silver Spring Ghosts

### Rebuild

再建中のチーム。

- 若手、指名権、低年俸を重視
- 高額FAは避ける
- ベテランを放出しやすい
- 勝利より育成を優先するシーズンがある

対象例：
- Fairmount Rams
- Bethesda Blaze

---

## 2. ドラフトAI

ドラフトでは、各チームが自分の哲学に合う選手を優先する。

### 評価軸

- 現在能力
- 将来性
- チームの弱点
- チーム哲学との相性
- 年齢
- ポジション需要
- 性格・安定感

### 哲学別ドラフト傾向

| 哲学 | ドラフト傾向 |
|---|---|
| Win Now | 即戦力、完成度の高い大学生、先発投手 |
| Balanced Baseball | 弱点が少ない総合型 |
| Player Development | 高ポテンシャルの素材型 |
| Power Baseball | 長打力、強肩、球威 |
| Small Ball | contact、eye、speed、守備 |
| Defense First | 捕手、遊撃手、中堅手、守備型選手 |
| Pitching First | 先発投手、制球、球威、耐久力 |
| Rebuild | 若さ、将来性、低コスト、素材型 |

---

## 3. FA AI

FAでは、チームの勝負年と資金力が大きく影響する。

### 評価軸

- 現在の実力
- チームの弱点を埋めるか
- 年齢リスク
- 年俸の重さ
- 契約年数
- チーム哲学との相性
- 人気・集客効果

### 哲学別FA傾向

| 哲学 | FA傾向 |
|---|---|
| Win Now | 高額スター選手にも積極的 |
| Balanced Baseball | 適正価格の安定型を好む |
| Player Development | 若いFA、成長余地のある選手 |
| Power Baseball | 長距離砲、速球派投手 |
| Small Ball | 出塁型、俊足、守備型 |
| Defense First | 守備型捕手、内野守備、中堅手 |
| Pitching First | 先発投手、リリーフ、捕手 |
| Rebuild | 短期契約、安いベテラン、転売候補 |

---

## 4. トレードAI

トレードはチームの現在地で変わる。

### チーム状態

| 状態 | 行動 |
|---|---|
| 優勝争い中 | 若手を出して即戦力を取る |
| ワイルドカード争い中 | 安い補強を探す |
| 中位 | 大きく動かず、必要な補強だけ |
| 再建中 | ベテランを売って若手を取る |
| 最下位争い | 年俸削減と将来資産集め |

### トレード傾向

- Bronxは優勝争いなら大胆に補強する
- Georgetownは無駄なトレードをしない
- Brooklynは長打力を求める
- Ghostsは投手コアを守る
- Foxes、Ramsは若手中心に集める。Stallionsも同様だが、特にcontact型に絞って集める
- Blazeは焦って少し損なトレードをする可能性がある
- Newark Knights、Manayunk Runnersは足を使える選手なら格下チームとも積極的に組む
- Kensington Ironは守備・投手の穴だけをピンポイントで埋め、派手な補強はしない
- Anacostia Kingsは投手力の弱点を埋めるトレードを常に探している
- Alexandria Cannons、Capitol Hill Senatorsは安定した中堅選手を好み、大きな入れ替えは避ける

---

## 5. 試合采配AI

試合中の采配は、teamStyle の数値で決まる。

### 判断項目

- 盗塁するか
- バントするか
- ヒットエンドランを使うか
- 投手を早く替えるか
- 守備シフトを使うか
- 代打を出すか
- 守備固めをするか

### 盗塁

盗塁を好むチーム：
- Manayunk Runners
- Bronx Wolves
- Harlem Eagles

盗塁が少ないチーム：
- Brooklyn Hammers
- Alexandria Cannons
- Silver Spring Ghosts

### バント

バントを好むチーム：
- Manayunk Runners
- Harlem Eagles
- Silver Spring Ghosts
- Fairmount Rams

バントしないチーム：
- Bronx Wolves
- Brooklyn Hammers
- Fishtown Ferals
- Anacostia Kings

### ヒットエンドラン

多用するチーム：
- Manayunk Runners
- Harlem Eagles
- Newark Knights

少ないチーム：
- Brooklyn Hammers
- Fishtown Ferals
- Anacostia Kings

### 継投

早めに投手を替えるチーム：
- Bronx Wolves
- Georgetown Ravens
- Fishtown Ferals
- Silver Spring Ghosts

先発を信頼するチーム：
- Germantown Colonials
- Newark Knights
- Alexandria Cannons

### 守備シフト

多く使うチーム：
- Silver Spring Ghosts
- Kensington Iron
- Georgetown Ravens
- Bronx Wolves

あまり使わないチーム：
- Manayunk Runners
- Harlem Eagles
- Fairmount Rams

---

## 6. 球団別AI方針

### Newark Knights
**哲学：Small Ball** — 足で試合を作るチーム。
- ドラフト：speed、contact、守備を重視
- FA：出塁型・俊足の選手を優先
- トレード：走塁を使える駒を集める
- 采配：単打・盗塁・状況打撃で確実に加点する

### Queens Titans
**哲学：Balanced Baseball** — 投打の軸を持つ伝統強豪。
- ドラフト：投手と打者をバランスよく指名
- FA：エース級や中軸打者を狙う
- トレード：必要な穴を的確に埋める
- 采配：極端なことはせず、王道で押す

### Brooklyn Hammers
**哲学：Power Baseball** — 長打で試合を壊すチーム。
- ドラフト：power、arm、exit velocity重視
- FA：長距離砲や速球派投手を好む
- トレード：打線強化に積極的
- 采配：バント少なめ、強打優先

### Bronx Wolves
**哲学：Win Now** — 勝利が義務の常勝軍団。
- ドラフト：即戦力、完成度重視
- FA：スター選手にも積極的
- トレード：優勝争いなら若手を放出する
- 采配：攻撃的、盗塁多め、勝ちパターンを早く使う

### Harlem Eagles
**哲学：Small Ball** — 美しくつなぐ野球。
- ドラフト：contact、eye、speed、守備
- FA：出塁型打者や守備型選手
- トレード：チームの流れを壊さない補強
- 采配：バント、ヒットエンドラン、状況打撃を多用

### Staten Island Foxes
**哲学：Player Development** — 将来性に賭ける反骨チーム。
- ドラフト：高ポテンシャル重視
- FA：若くて安い選手を狙う
- トレード：ベテランより若手を優先
- 采配：勝利より経験を重視する場面がある

### Fishtown Ferals
**哲学：Power Baseball** — 野性的で爆発力のあるチーム。
- ドラフト：荒くても天井が高い選手
- FA：パワーヒッター、球威型投手
- トレード：勢いを増す補強を好む
- 采配：攻撃的で、試合の流れを重視

### Kensington Iron
**哲学：Defense First** — 硬く、崩れないチーム。
- ドラフト：守備、耐久性、制球
- FA：守備型選手、安定した投手
- トレード：派手さより弱点補強
- 采配：守備固め、シフト、堅実な継投

### Germantown Colonials
**哲学：Balanced Baseball** — 伝統と経験を重んじるチーム。
- ドラフト：完成度の高い選手、制球型投手
- FA：ベテランも評価
- トレード：急激な再建より現有戦力を信頼
- 采配：先発を信じ、昔ながらの野球をする

### Manayunk Runners
**哲学：Small Ball** — 走力で相手を壊すチーム。
- ドラフト：speed最優先、contactも重視
- FA：俊足、出塁型、守備型
- トレード：足を使える選手を集める
- 采配：盗塁、バント、ヒットエンドランを多用

### Fairmount Rams
**哲学：Rebuild** — 若手中心の再建チーム。
- ドラフト：将来性、素材型、academy向きの選手
- FA：短期契約中心
- トレード：ベテランを若手に変える
- 采配：若手に出場機会を与える

### South Philly Stallions
**哲学：Player Development** — コンタクト型の若手に賭ける再建チーム。
- ドラフト：contact型の素材選手を重視
- FA：地元人気選手や短期のつなぎ補強のみ
- トレード：即戦力より将来性を優先して集める
- 采配：若手の経験を優先しつつ、昔ながらの堅実さも残す

### Georgetown Ravens
**哲学：Win Now** — 知性と組織力で勝つエリート球団。
- ドラフト：完成度、野球IQ、弱点の少なさ
- FA：必要なピースだけを正確に補強
- トレード：無駄が少なく、計算された動き
- 采配：シフト、継投、代打を合理的に使う

### Capitol Hill Senators
**哲学：Balanced Baseball** — 正統派の野球を貫くチーム。
- ドラフト：基本能力が高い選手
- FA：リスクの低い安定型
- トレード：奇策より堅実補強
- 采配：オーソドックスで、無理をしない

### Anacostia Kings
**哲学：Power Baseball** — 反骨心と打力で押し切るチーム。
- ドラフト：power最優先
- FA：強打者の補強に積極的
- トレード：投手より打線強化に寄りやすい
- 采配：強打優先、守備リスクも受け入れる

### Alexandria Cannons
**哲学：Balanced Baseball** — ベテランと安定感のチーム。
- ドラフト：即戦力寄り
- FA：経験豊富な中堅・ベテランを評価
- トレード：大きな賭けより安定補強
- 采配：大崩れしない堅実な試合運び

### Bethesda Blaze
**哲学：Rebuild** — 若く不安定で、まだ迷走中の再建チーム。
- ドラフト：若さ、将来性、身体能力を優先しつつ即戦力にも手を出しがち
- FA：本来は避けるべき若手FAに、焦って過払いすることがある
- トレード：ベテランを放出すべきだが、短期的な補強に揺れる
- 采配：まだ一貫性が弱く、試合ごとにムラがある

### Silver Spring Ghosts
**哲学：Pitching First** — 投手で試合を支配するチーム。
- ドラフト：先発投手、制球、守備型捕手
- FA：投手、守備、リリーフに投資
- トレード：投手コアは簡単に売らない
- 采配：継投、守備シフト、ロースコア勝負を重視

---

## 7. 最終イメージ

JBLのチームAIは、ただ強い選手を集めるのではなく、球団ごとに違う考え方で動く。

- Bronxは勝つために金と若手を使う
- Brooklynは長打を集める
- Harlemは美しくつなぐ
- Manayunkは走る
- Ghostsは投手で沈黙させる
- Ravensは計算で勝つ
- FoxesとStallionsはコンタクト型の若手に賭けて未来を育てる
- Ramsは実力で劣っても諦めずに未来を育てる

この設計なら、同じリーグの中でも各球団がまったく違う動きをするようになる。

---

## 8. 参考：チームの雰囲気（地区別トーン）

地名から連想される雰囲気の参考メモ。ロゴやブランディングのタッチ（角ばった攻撃的な線 vs 柔らかい伝統的な紋章風など）の使い分けに使える。

### 北地区（ニューヨーク）— 荒々しい・ストリート系

| チーム | 性格 |
|---|---|
| Brooklyn Hammers | 労働者階級、無骨で叩き上げ。工場・港湾のタフさ |
| Bronx Wolves | 攻撃的で群れで襲う。荒っぽく威圧的 |
| Queens Titans | 移民街の多様さを背負った、力強く誇り高い巨人 |
| Harlem Eagles | 誇り高くカリスマ的、上から見下ろす威厳 |
| Staten Island Foxes | 少しアウトサイダー、ずる賢く俊敏 |
| Newark Knights | 古き良き騎士道、規律と名誉を重んじる |

### 中地区（フィラデルフィア）— 下町・叩き上げ系

| チーム | 性格 |
|---|---|
| Fishtown Ferals | 野性的でストリート育ち、荒削りだが速い |
| Kensington Iron | 工業地帯の頑丈さ、無骨で壊れない |
| Germantown Colonials | 独立戦争の歴史を背負う、伝統と反骨精神 |
| Manayunk Runners | 坂の多い街を走り抜くスタミナ、地道な努力型 |
| Fairmount Rams | 公園と美術館の街、力強いが品もある |
| South Philly Stallions | 荒々しい競走馬、勝負師気質 |

### 南地区（DC/MD/VA）— 上品・権威系

| チーム | 性格 |
|---|---|
| Georgetown Ravens | 知的でミステリアス、大学街らしい洗練さ |
| Capitol Hill Senators | 政治の中枢らしい貫禄と駆け引き上手 |
| Anacostia Kings | 王者の風格、地域コミュニティの誇り |
| Alexandria Cannons | 歴史ある港町、重厚で一撃必殺 |
| Bethesda Blaze | 郊外の熱気、燃え上がる勢いのある新興チーム |
| Silver Spring Ghosts | 掴みどころのない不気味さ、静かに忍び寄る強さ |

全体として「北地区=荒々しいストリート」「中地区=下町の叩き上げ」「南地区=上品・権威」という3段階のトーンの違いがある。

---

## 9. 参考：Teams.cpp に既存の実力面の球団設定

`src/Teams.cpp` の各球団定義コメントに書かれている、プレースタイル・実力面のキャラクター（原文ママ）。上記のAI設計を実装する際は、この既存設定と矛盾しないようにする。

| チーム | 特徴 |
|---|---|
| Newark Knights | スピード&コンタクト型。足で点を取る。先発ローテが安定 |
| Queens Titans | 投打のバランスが取れた名門。左腕エースと強打者を軸に、常に優勝争いに絡む。かつての覇者としての誇り |
| Brooklyn Hammers | 長打力と強力なブルペンを武器にする打撃型チーム。Iron Yardのファンは最もうるさいことで知られる |
| Bronx Wolves | JBL最多優勝を誇る常勝軍団。エース・クローザーともにリーグ屈指の投手陣、打線も破壊力抜群 |
| Harlem Eagles | コンタクト&スピードを磨いた伝統的スモールボール球団。打率・出塁率が高い |
| Staten Island Foxes | 若手主体で再建中。先発陣は未完成だが育成力は高い |
| Fishtown Ferals | 強力な先発・ブルペンとパワーヒッターを兼ね備えた上位常連。荒削りだが破壊力のある攻撃 |
| Kensington Iron | 堅実な投手陣と粘り強い打線。守備が固く、コツコツ点を取る |
| Germantown Colonials | 攻守平均的、制球型の先発が多いオーソドックスな伝統球団 |
| Manayunk Runners | 全員快足のリーグ一のスモールボール球団。盗塁・バント・エンドランで揺さぶる |
| Fairmount Rams | 実力は劣るが諦めないアンダードッグ、気持ちで戦う |
| South Philly Stallions | 再建途中だがコンタクト型の若手が多く、数年後に化ける可能性 |
| Georgetown Ravens | 地区最強、完成度の高い名門。優勝以外は失敗とされる |
| Capitol Hill Senators | 基本に忠実な伝統球団。派手さはないが大崩れしない |
| Anacostia Kings | 爆発力はリーグ屈指、投手力に課題 |
| Alexandria Cannons | 攻守平均的、突出した武器はないが堅実 |
| Bethesda Blaze | 再建途中の若いチーム、まだ安定した戦い方を見つけられていない |
| Silver Spring Ghosts | 投手力と守備力で接戦をものにする玄人好みの球団 |
