# SaitekiKuukan(最適空間)
> 高性能な C++ 向け Data-Oriented Design (DOD) ライブラリ

`saitekikuukan` は、ゲームエンジンやシミュレーション向けに設計された **データ指向のコンテナライブラリ** です。  
型パラメータパックやリフレクションを使って SoA と、固定容量の SoA を束ねたチャンク型（AoSoA）を扱えます。

---

## 特徴

- **Data-Oriented Design 対応** ✅  
  - SoA の列単位のアクセスで CPU キャッシュを活用

- **型パック操作・メタプログラミング** ✅  
  - 型リスト (`type_list`) や条件フィルタ (`_filter`) を使った静的型操作  
  - `if constexpr` と `concepts` による安全で可読性の高いコード

- **最新 C++ 機能対応** ✅  
  - 型パック版は C++23、構造体の自動展開は C++26 リフレクションに対応
  - `std::expected`, `constexpr`, `concepts` を活用

- **省メモリ設計** ✅  
  - Stateless 型の EBO に対応  
  - ID なしの SoA と、ID・イベント付きの buffer を用途に応じて選択

- **高速化・最適化** 
  - 連続した列へのアクセスを提供。自動 SIMD 化はコンパイラと処理内容に依存

- **ヘッダオンリー** ✅  
  - 1 ファイルで完結  
  - 簡単にインクルードして使用可能

---

## インストール

ヘッダオンリーなので、プロジェクトに `include/saitekikuukan.hpp` をコピーしてください！

`add_subdirectory`、またはインストール後の `find_package(saitekikuukan CONFIG REQUIRED)` にも対応します。
通常は `saitekikuukan::saitekikuukan`、構造体を自動展開する場合は `saitekikuukan::reflection` をリンクします。
親プロジェクトの C++ バージョンや最適化フラグは変更しません。

## buffer と方針の組み合わせ

`buffer<Schema, Structure, Layout, Identity, Events>` を共通の入口にします。
スキーマ・データ配置・格納方式・ID管理・イベントを、それぞれ型で選択します。

| 引数 | 既定値 | 選択肢 |
| --- | --- | --- |
| Schema | `type_list<>` | `type_list<Types...>`、構造体（C++26 reflection） |
| Structure | `strategy::structure_of_array` | 列の所有と行・列アクセスを提供する配置方針 |
| Layout | `storage::dynamic_layout<>` | `fixed_layout<N>`、`dynamic_layout<N>`、`chunked_layout<N>` |
| Identity | `identity::none` | `identity::generated<Generator, Lookup>` |
| Events | `eventsystem::dummy` | 登録型リスナ、独自イベントシステムなど |

`soa`・`fixed_soa`・`record_soa`・`chunked_soa` は、設定済みの `buffer` を表す型エイリアスです。
IDの有無はデータ配置と独立しており、SoAとChunkのどちらにもID管理を組み合わせられます。
IDなしの場合は、ID列・ジェネレータ・検索索引を保持しません。

追加とサイズ変更は `expected<void, error_code>` を返します。容量超過は `buffer_over` です。
確保や要素の構築・代入が投げた例外は伝播します。

## 列ビューと行アクセス

```cpp
// C++23: 型パックと構造化束縛
kk::buffer<kk::type_list<float, float, std::string>,
    kk::strategy::structure_of_array> values;
values.push_back(1.0f, 2.0f, "first").value();
auto [x, y, name] = values.columns();
x[0] += 1.0f;
for (auto [rx, ry, label] : values)
    ry += rx;

// 固定容量でも操作は共通。有効行数は容量と別に管理する。
kk::fixed_soa<1024, float, float> fixed;
fixed.resize(10).value();
```

```cpp
// C++26: 構造体から同じ仕組みに列名と行参照を生成
struct vertex {
    float x = 0, y = 0;
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};
kk::buffer<vertex> vertices;
vertices.resize(10).value();                 // vertex{} で初期化。normal[2] は 1
auto storage = vertices.columns();   // 所有列へのビューを値で取得
storage.x[0] = 1.0f;
auto [vx, vy, normals, names] = storage;
vertices.begin()->x += 1.0f;
vertices.column<^^vertex::y>()[0] = 2.0f;
vertex value = vertices.read(0);     // 所有する構造体の値
vertices.write(1, value).value();

using fixed_vertices = kk::record_soa<vertex, kk::storage::fixed_layout<1024>>;
// 上限付き可変長: kk::storage::dynamic_layout<1024>
```

`column<N>()` / `get<N>()` は有効行だけのビューを返します。連続した通常の列は `std::span<T>`、
const なコンテナでは `std::span<const T>` です。`std::vector<bool>` や非連続列は対応するイテレータのビューになります。
`columns()` のメンバ名と順序は構造体と同じです。各ビューの長さを変更しても所有列の長さは変わりません。
行の `operator[]` は境界検査なし、`at`・`read`・`write`・`erase_unordered` は境界検査ありです。
`erase_unordered(i)` は末尾行を移して削除するため、順序を保持しません。
行イテレータはランダムアクセスの参照プロキシで、行の値の読み書きには `read` / `write` を使います。
`soa<>` や空の構造体でも行数を保持します。ID なしの構造体には `id` などの予約メンバ名はありません。

固定長・可変長の `Layout` は `column<T>`・`resizable`・`max_size` を持つ型です。独自の列コンテナも同じ行管理に接続できます。
列にはランダムアクセスと `size` が必要で、可変長列には `push_back`・`pop_back`・`clear` も必要です。
`reserve` / `capacity` / `max_size` があれば利用します。固定長列は容量分の要素を最初に構築します。
可変長列の要素にはコピー構築か例外を投げないムーブ構築が必要です。

追加前に全列の引数を退避するため、既存列を引数にした追加にも対応します。追加や拡張中に例外が起きても、行数と列長の整合性を保ちます。
標準の可変長列のコピー代入は、コピーを完了してから置き換えます。
要素の代入・交換が例外を投げる場合、`write`・`erase_unordered`・固定長列の代入は一部の値が変わることがあります。
再確保などで参照が無効になる操作後は列ビューや行参照を取得し直してください。
`reserve` や失敗した拡張でも、一部の列が再確保される場合があります。
一時的な SoA から列ビューを取得する操作は拒否します。
独自列コンテナのムーブが例外を投げる場合、整合性を保つためムーブ元を空にすることがあります。

## チャンク単位の SoA（AoSoA）

`storage::chunked_layout<ChunkSize>` は、固定容量のSoAをブロックとして束ねます。
`chunked_soa<ChunkSize, Types...>` は、そのIDなし設定のエイリアスです。
各ブロックを個別に保持するため、追加や `reserve` で既存行を移動させません。
`ChunkSize` は正のコンパイル時定数で、2の累乗以外も指定できます。

```cpp
// C++23: 型パック
kk::chunked_soa<128, float, float> points;
points.resize(300).value(); // 有効なチャンクは128行・128行・44行
for (auto chunk : points.chunks()) {
    auto [x, y] = chunk.columns();
    for (std::size_t i = 0; i < chunk.size(); ++i) {
        x[i] += 1.0f;
        y[i] += 2.0f;
    }
}
points.column<0>()[129] = 10.0f;
points.erase_unordered(0).value(); // 全体の末尾行を移して削除
```

```cpp
// C++26: 既存と同じスキーマ生成を利用
struct particle { float x = 0, y = 0; std::string name = "default"; };
kk::record_chunked_soa<particle, 128> particles;
particles.resize(300).value();
for (auto chunk : particles.chunks()) {
    auto storage = chunk.columns();
    for (std::size_t i = 0; i < chunk.size(); ++i)
        storage.x[i] += storage.y[i];
}
particles.begin()->name = "first";
particles.column<^^particle::x>()[129] = 2.0f;
```

全体の `begin/end`・`operator[]`・`at`・`read/write` はチャンクをまたいでアクセスします。
`column<N>()` / `get<N>()` も全体をランダムアクセスできるビューですが、連続範囲ではありません。
`chunks()` / `chunk(i)` が返すビューでは、各列が有効行だけの `std::span` になります。
SIMD化を狙う走査は、上の例のようにチャンクごとの連続列を使います。
個別チャンクのビューからは要素だけを更新でき、行数はコンテナ側で管理します。
const なコンテナの列・行・チャンクは、いずれも const な要素を参照します。

`reserve(n)` は必要なチャンクを構築し、`capacity()` は確保済みの行容量、`chunk_count()` は有効行を持つチャンク数を返します。
通知を伴わない `resize(n)` は各行を元の構造体のデフォルト値で初期化し、失敗時は全体の行数を戻します。
固定長 SoA と同じく容量分の列要素を構築するため、列の要素にはデフォルト構築が必要です。
`pop_back`・`clear`・縮小後も未使用スロットのオブジェクトを保持し、`shrink_to_fit()` で未使用チャンクを解放します。
コピー可能な列ならコピーに対応し、move-only の列も保持できます。
要素の代入・交換が例外を投げる場合の `write` / `erase_unordered` は一部の値が変わることがあります。
ビューの範囲は取得時点の行数なので、サイズの変更後は取り直してください。

## ID管理と検索方式

```cpp
struct particle { float x = 0, y = 0; std::string name; };
using particles_type = kk::buffer<particle,
    kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<1024>,
    kk::identity::generated<>>;

particles_type particles;
particles.push_back(particle{1, 2, "first"}).value();
auto id = particles.begin()->id();
particles.find(id).value().x = 5;
particles[0].y = 10; // 添字は常に物理行インデックス
particles.erase(id).value();
```

`identity::generated<Generator, Lookup>` はID生成と検索方式を別々に指定します。
既定は `generator::iota` と `strategy::hash_id_lookup` です。
生成器は `generate_type` と、`optional<generate_type>` を返す `next()` を持ちます。

| 検索方式 | 特徴 |
| --- | --- |
| `strategy::hash_id_lookup` | ハッシュ索引を追加し、検索・ID登録は平均O(1)。削除や行交換でも索引を更新 |
| `strategy::linear_id_lookup` | 索引を保持せずID列を走査。検索・重複確認はO(n)、まとめた拡張の重複確認はO(n²) |

ハッシュ方式には例外を投げない `std::hash<ID>` と等値比較が必要です。
ID型にはコピー構築と、例外を投げないムーブ・交換が必要です。固定長ではID型もデフォルト構築可能である必要があります。
両方式とも重複IDを `duplicate_id` として拒否します。
生成失敗は `generate_id_failed` です。失敗やキャンセルでも生成器は進むため、IDには欠番が生じます。

`find(id)` と `index(id)` は `expected` を返し、見つからない場合は `not_found_id` です。
`get<N>(id)` は通常の列で `expected<reference_wrapper<T>, error_code>` を返します。
`operator[]`・`at`・`read/write`・`erase_unordered` の引数は、IDの有無によらず物理行インデックスです。
`erase(id)` はIDによる削除です。末尾行を移動しても、その行のIDを維持します。
所有列とID表は非公開で、`columns()`・`column<N>()`・`get<N>()` は要素を更新できるビューです。
`capacity()` はデータ列の容量を返し、ID列とハッシュ索引の確保は追加で発生します。
ID付きの行では `id`・`get`・`parent`・`index` が行APIの予約名です。

## コルーチンによるID生成

`generator::coroutine_generator<T>` は `co_yield` した値を `optional<T>` で返します。
空・終了後・ムーブ元の生成器は `nullopt` を返し、コルーチン内部の例外は `next()` から伝播します。
ハンドルを単独所有するムーブ専用型で、`T` にmove-only型も使えます。

状態を持つ生成器は、ID付き `buffer` のコンストラクタへ渡せます。
第2引数にイベントシステムの初期値も指定できます。

```cpp
using ids = kk::generator::coroutine_generator<std::size_t>;
ids make_ids() {
    co_yield 10;
    co_yield 20;
}
using values_type = kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<>, kk::identity::generated<ids>>;
values_type values{make_ids()};
values.push_back(7).value();
```

## Sparse索引

整数IDには `strategy::sparse_id_lookup<PageSize = 256, Key = strategy::sparse_key>` も使えます。
ハッシュを使わず、ページ化した配列でIDから物理行番号を直接引きます。
既定のキーは符号なし整数の値、またはIDの `slot()` が返す符号なし整数です。
独自の `Key` は例外を投げずに符号なし整数を返す関数オブジェクトです。
同じキーを持つIDは同時に格納できず、検索時はID全体も照合します。

```cpp
using ids = kk::identity::generated<kk::generator::iota, kk::strategy::sparse_id_lookup<>>;
kk::buffer<kk::type_list<float>, kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<>, ids> values;
```

要素用ページは必要になった時だけ確保します。外側のページ表は最大キーに応じて伸びるため、
極端に疎な巨大IDにはハッシュ方式が適します。`reserve(n)` はページ表の予約を行い、
`clear` は確保済みのページを再利用します。ハッシュ方式も平均O(1)なので、速度・メモリは用途ごとに比較します。
## IDの再利用と世代管理

`identity::recycled<Lookup = strategy::hash_id_lookup, Index = uint32_t, Generation = uint32_t>` は、
`generator::recycling` と検索方式を組み合わせます。`identity::generated` の仕組みで配置・格納方式と独立に選べます。
IDは `identity::generational_id<Index, Generation>` で、`slot()` と `generation()` を持ちます。

```cpp
kk::buffer<kk::type_list<int>, kk::strategy::structure_of_array,
    kk::storage::dynamic_layout<>, kk::identity::recycled<>> values;
values.push_back(7).value();
auto old = values.begin()->id();
values.erase(old).value();
values.push_back(9).value();
// スロットは再利用されるが世代が変わるので、古い ID は見つからない。
assert(!values.find(old));
```

削除・縮小・clearでIDを解放し、次回発行時にスロットを再利用します。
世代の上限に達したスロットは永久に再利用せず、世代を巻き戻しません。
Indexの最大値は無効ID用に予約されます。スロットを使い切ると `generate_id_failed` です。
コピーは独立した寿命管理状態を複製し、ムーブは状態を移譲します。ムーブ元の生成器は未接続となり、新規発行を行いません。
IDは対応するbufferの状態に対して使用してください。

追加前のキャンセル・例外や、一括拡張の失敗でも予約IDを解放します。
追加後イベントの例外では挿入済みIDを解放しません。再利用型の追加前イベントでIDを書き換えると `invalid_id` になります。
IDを返却できる独自生成器は `release(const ID&) noexcept` を持ち、IDのコピー構築はnoexceptである必要があります。
通常の `iota` など、返却を持たない生成器にはこの寿命管理を追加しません。

## 行単位のソートと一括操作

```cpp
kk::soa<int, std::string> values;
std::array rows{std::tuple{3, std::string("three")}, std::tuple{1, std::string("one")}};
values.append_range(rows).value();
values.sort(std::ranges::less{}, [](auto row) { return std::get<0>(row); }).value();
auto removed = values.erase_if([](auto row) { return std::get<0>(row) == 1; }).value();
```

- `sort(compare, projection = std::identity{})` は読み取り専用の行を比較し、全列・IDをまとめて並べ替えます。
  構造体の行では `[](auto row) { return row.x; }` などをprojectionに渡せます。安定ソートではありません。
  比較・projectionの例外は行の移動前に伝播します。並べ替え用に行数分のindex配列を2本使用し、行の値自体はコピーしません。
- `append_range(rows)` は元の行型へ変換できる入力rangeを受け取り、成功時に追加件数を返します。
  bufferを渡す場合は格納方式・ID設定が違っても同じ行型ならコピーできます。元のIDはコピーせず、新しいIDを生成します。
  入力を先に所有値へ退避するため、自己追加や自分の列ビューからの追加でも再確保後の参照を使いません。
  move-only値はmove_iteratorなど、値を移動できるrangeで渡します。退避用メモリは入力件数に比例します。
  入力の読み取り・退避・容量検査の失敗では行を追加しません。
- `erase_if(predicate)` は読み取り専用の行を判定し、一致する行を順不同で削除して成功時に削除件数を返します。

`sort` は `expected<void, error_code>`、`append_range` / `erase_if` は `expected<size_t, error_code>` を返します。
操作は公開の追加・交換・削除APIを通り、対応するイベントとIDの寿命管理も実行します。
キャンセルや変更中の例外では停止し、それまでの変更を保持します。バッチ全体のロールバックは行いません。
失敗時の追加・削除件数は呼び出し前後のsizeから確認できます。
比較・projection・predicate・入力rangeの処理中は対象bufferの行数や並び順を変更しないでください。

## イベント

最後のテンプレート引数で `Events` を指定し、`events()` からイベントシステムを参照します。
IDの有無、固定長・可変長・Chunkのいずれでも、追加・削除・交換・更新の前後を通知できます。

| イベントシステム | リスナの保持と呼び出し |
| --- | --- |
| `dummy` | 通知なし。イベントのための状態や行の退避を追加しない |
| `registration<Events...>` | 外部リスナを登録・解除。同じイベントの登録順に呼び出す |
| `one_event_each_only<Events...>` | イベントごとに外部リスナを一つ登録 |
| `zero_cost<Listeners...>` | リスナを保持せず、通知時に一時生成。型の宣言順に呼び出す |
| `minimum_cost<Listeners...>` | 状態を持つリスナだけを保持し、空のリスナと合わせて宣言順に呼び出す |

`zero_cost` / `minimum_cost` のリスナは `listen_event` と `on(Event&)` を持ちます。
`listener_tag<Event>` を継承して宣言でき、空のリスナ一覧や同じイベントを購読する複数のリスナにも対応します。
`minimum_cost` の状態を持つリスナは、宣言順に `events().listeners` のtupleへ格納します。

| 操作 | IDあり | IDなし |
| --- | --- | --- |
| 追加 | `will_insert_event<ID, Types...>` / `did_insert_event<ID, Types...>` | `will_insert_row_event<Types...>` / `did_insert_row_event<Types...>` |
| 削除 | `will_delete_event<ID, Types...>` / `did_delete_event<ID, Types...>` | `will_delete_row_event<Types...>` / `did_delete_row_event<Types...>` |
| 行交換 | `will_swap_event<ID, Types...>` / `did_swap_event<ID, Types...>` | `will_swap_row_event<Types...>` / `did_swap_row_event<Types...>` |
| 更新 | `will_update_event<ID, Types...>` / `did_update_event<ID, Types...>` | `will_update_row_event<Types...>` / `did_update_row_event<Types...>` |

`will_*` は `cancel()` で操作を取り消せます。取り消した行操作は `operation_canceled` を返し、対応する `did_*` は通知しません。
追加・更新前の `data` は格納予定の値への参照で、リスナから修正できます。
削除・交換前の行は読み取り専用です。追加時の生成IDは変更できますが、削除・交換・更新のIDは読み取り専用です。
追加・更新・交換後の参照は格納済みの要素を指し、リスナから修正できます。`vector<bool>` の値も書き戻します。

削除前は `index` と `data`、IDありなら `id` を通知します。
削除後は削除位置の `index` と、IDありなら削除された `id` だけを通知します。
破棄済みの要素への参照を渡さないため、move-onlyの列でも削除通知を使えます。
交換時の `first_index` / `second_index`、`first` / `second` は指定位置の行を表します。
IDありの `first_id` / `second_id` も、その通知時点で各位置にあるIDです。

- `erase` / `erase_unordered` / `pop_back` は削除イベントを通知します。削除に伴う内部移動では交換イベントを通知しません。
- `swap_rows` は交換イベントを通知します。同じ位置同士の交換は通知しません。
- `write` は更新イベントを通知します。列ビューや行参照への直接代入は通知しません。
- `resize` の拡張は追加イベント、縮小と `clear` は末尾から削除イベントを通知します。
- 空の `pop_back` / `clear`、サイズを変えない `resize` は通知しません。
- `reserve`、コンテナ全体のコピー・ムーブ・交換、デストラクタは行イベントを通知しません。

`push_back` / `resize` / `write` / `erase` / `erase_unordered` / `pop_back` / `clear` / `swap_rows` は
`expected<void, error_code>` を返します。行位置の境界検査による `out_of_range` 例外は従来と同じです。
通知対象のない操作は、従来の格納処理を直接使います。

通知を伴う `resize` / `clear` は行ごとに処理し、キャンセルや例外が起きた時点で停止します。
それまでに完了した行操作と通知は維持されます。`did_*` のリスナが例外を投げた場合も、対応する行操作自体は完了しています。

外部登録型はリスナを所有しないため、登録中のリスナを生存させてください。
通知中は同じbufferの行数や並び順を変更する操作を呼び出さず、通知先のリスナ一覧も変更しないでください。

## 列ストレージだけを生成する

`saitekikuukan.hpp` から、構造体の各メンバを同名の列配列に変換する API も使えます。
配列そのものをメンバとして保持し、`storage.x` と構造化束縛の両方でアクセスできます。

```cpp
#include "saitekikuukan.hpp"
#include <string>

struct particle {
    float x, y;
    std::string name;
};

kk::storage::static_soa<particle, 1024> storage{};
storage.x[0] = 1.0f;
storage.name[0] = "first";
auto& [x, y, name] = storage;

kk::storage::dynamic_soa<particle> dynamic;
kk::storage::for_each_column(dynamic, [](auto& column) { column.resize(1024); });
dynamic.x[0] = 2.0f;
```

行単位の走査には `rows(storage)` を使います。`begin()` / `end()` とランダムアクセスに対応し、
各行は元の列要素への参照／プロキシを、同じメンバ名で保持します。

```cpp
auto rows = kk::storage::rows(storage);
rows.begin()->x += 1.0f;
auto row = *rows.begin();
row.name = "updated";

for (auto row : rows) {
    row.x += 1.0f;
    row.y += 2.0f;
}

auto readonly = kk::storage::rows(std::as_const(storage));
```

`auto row` で受けても変更は元の列に反映されます。const なストレージでは列の const アクセス結果を使います。
可変長の列の長さが異なる場合は最短の列長まで走査し、メンバのない構造体は 0 行として扱います。
列を resize した場合はビューを取得し直してください。一時的なストレージからのビュー生成は拒否します。

固定長版は `std::array`、可変長版は `std::vector` を生成します。
独自の列コンテナには `kk::storage::soa<Record, Column>` を使えます。
列ストレージだけを生成する API で、既存の `buffer` の ID 管理やイベント処理は含みません。
各列の長さは利用側で管理し、元の構造体のメンバ初期化子は引き継ぎません。
スカラ列もゼロ初期化する場合は、固定長版の宣言に `{}` を付けてください。

対象は基底クラス・参照メンバ・ビットフィールドを持たない、union 以外の集成体です。
静的メンバは列に含まれず、ネストした構造体は一つの列の要素型になります。
GCC 16 では `-std=c++26 -freflection` が必要です。リフレクションを使わない従来形式は通常の設定でビルドできます。
実装は `saitekikuukan.hpp` 一つにまとまっています。

```sh
cmake -S . -B build/reflection -DCMAKE_CXX_COMPILER=g++-16 \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/reflection
ctest --test-dir build/reflection --output-on-failure
```

通常のテストは `SAITEKIKUUKAN_BUILD_TESTS`、ベンチマークは `SAITEKIKUUKAN_BUILD_BENCHMARKS` で切り替えます。
型パック版は C++23 でビルドできます。

---

## 実行できるサンプル

[test/samples](test/samples/README.md) に、用途別の使用例を用意しています。
型パック・固定容量・IDとイベント・Chunk・独自列・move-only型と、構造体reflectionの例を
個別にビルドでき、CTestで結果を確認できます。

## 性能の継続確認

`tools/performance_report.py` は、動的 SoA と直接の vector を同じ入力で比較します。
確保済み容量での `clear→resize`・`clear→push_back` と、列・行・`read/write` の走査を計測します。
各操作の結果を全行検査し、初回の容量確保を除いた ns/行の中央値を CSV に保存します。

```sh
python3 tools/performance_report.py --compiler clang++
python3 tools/performance_report.py --compiler g++-16 --reflection
```

`--rows`・`--iterations`・`--samples` で入力と反復数、`--levels O3` で最適化レベルを選べます。
既定値は 262144 行・24 回・7 試行、最適化レベルは `-O2` / `-O3` です。
`--include-dir` に比較対象の版の include ディレクトリを指定すれば、同じ計測コードで旧版も測れます。
`--output` の既定値は `build/performance` で、生成コード・ベクトル化診断・コンパイラと引数・ソースのハッシュも保存します。
reflection を使う場合は、通常の構造体とメンバ初期化子のある構造体を別々に計測します。

CMake では `SAITEKIKUUKAN_BUILD_BENCHMARKS=ON` で `performance-benchmark` をビルドできます。
reflection のテストも有効な場合は `performance-reflection-benchmark` が加わります。
計測対象は別翻訳単位に分け、LTO による計測処理全体の定数畳み込みを避けています。

CI は Clang の C++23 と GCC 16 の reflection をサニタイザ付きで検証し、最適化別の性能レポートを artifact に保存します。
自動実行は GitHub Release の公開時（プレリリースを含む）で、Actions 画面からの手動実行にも対応します。
共有ランナーの速度変動があるため、時間の固定閾値では失敗させず、CSV と生成コードで退行を確認します。
速度はサニタイザなしの実行で比較し、コンパイラ・入力・マシン条件を揃えてください。

## 将来的な機能 (チェックリスト)

- [ ] SIMD 最適化  
- [ ] GPU 並列処理  

---
