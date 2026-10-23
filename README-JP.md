# SaitekiKuukan(最適空間)
> 高性能な C++ 向け Data-Oriented Design (DOD) ライブラリ

`saitekikuukan` は、ゲームエンジンやシミュレーション向けに設計された **データ指向のコンテナライブラリ** です。  
型パラメータパックやリフレクションを使って SoA を扱えます。AoSoA・チャンク型は構想段階です。

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

## ID なしの汎用 SoA

`basic_soa<Layout, Types...>` が列の所有と行数を管理します。ID・イベント付きの既存 `buffer` も同じ列管理を使います。
スキーマの指定と列コンテナの選択は独立しており、Mesh などの用途固有の型は必要ありません。

```cpp
// C++23: 型パックと構造化束縛
kk::soa<float, float, std::string> values;
values.push_back(1.0f, 2.0f, "first");
auto [x, y, name] = values.columns();
x[0] += 1.0f;
for (auto [rx, ry, label] : values)
    ry += rx;

// 固定容量でも操作は共通。有効行数は容量と別に管理する。
kk::fixed_soa<1024, float, float> fixed;
fixed.resize(10);
```

```cpp
// C++26: 構造体から同じ仕組みに列名と行参照を生成
struct vertex {
    float x = 0, y = 0;
    std::array<float, 3> normal{0, 0, 1};
    std::string name = "default";
};
kk::record_soa<vertex> vertices;
vertices.resize(10);                 // vertex{} で初期化。normal[2] は 1
auto storage = vertices.columns();   // 所有列へのビューを値で取得
storage.x[0] = 1.0f;
auto [vx, vy, normals, names] = storage;
vertices.begin()->x += 1.0f;
vertices.column<^^vertex::y>()[0] = 2.0f;
vertex value = vertices.read(0);     // 所有する構造体の値
vertices.write(1, value);

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

`Layout` は `column<T>`・`resizable`・`max_size` を持つ型です。独自の列コンテナも同じ行管理に接続できます。
列にはランダムアクセスと `size` が必要で、可変長列には `push_back`・`pop_back`・`clear` も必要です。
`reserve` / `capacity` / `max_size` があれば利用します。固定長列は容量分の要素を最初に構築します。
可変長列の要素にはコピー構築か例外を投げないムーブ構築が必要です。

追加前に全列の引数を退避するため、既存列を引数にした追加にも対応します。追加や拡張中に例外が起きても行数と列長を保ちます。
標準の可変長列のコピー代入は、コピーを完了してから置き換えます。
要素の代入・交換が例外を投げる場合、`write`・`erase_unordered`・固定長列の代入は一部の値が変わることがあります。
再確保などで参照が無効になる操作後は列ビューや行参照を取得し直してください。
`reserve` や失敗した拡張でも、一部の列が再確保される場合があります。
一時的な SoA から列ビューを取得する操作は拒否します。
独自列コンテナのムーブが例外を投げる場合、整合性を保つためムーブ元を空にすることがあります。

## 構造体を buffer のスキーマにする（C++26）

`record_buffer<T>` は既存の `buffer` に `record<T>` スキーマを渡すエイリアスです。
構造体から名前付きの列と行参照を生成し、要素数・ID・挿入・削除・イテレータを既存の経路で管理します。

```cpp
#include "saitekikuukan.hpp"
#include <string>

struct particle {
    float x, y;
    std::string name;
};

kk::record_buffer<particle> particles{};
auto inserted = particles.push_back(particle{1.0f, 2.0f, "first"});
auto& storage = particles.storage.entries;
storage.x[0] += 1.0f;
auto& [x, y, name] = storage;

auto it = particles.begin();
it->x += 1.0f;
it->get<0>() += 1.0f;
auto id = it->id();
particles.get<0>(id).value().get() = 5.0f;
for (auto row : particles)
    row.y += 2.0f;
```

固定長列の容量と、有効な行数 `particles.size()` は別です。`begin()` / `end()` は有効な行だけを走査します。
可変長版は `record_buffer<particle, generator::iota, eventsystem::dummy, storage::dynamic_storage>` で選べます。
従来の `buffer<Generator, Events, Storage, Size, Types...>` とタプルの `storage.entries` もそのまま使えます。
`record<T>` を明示しない構造体は従来どおり一つの列の要素型として扱います。

`push_back` は構造体と列ごとの引数の両方に対応します。`events()` から既存のイベント登録クラスを設定でき、
挿入前の変更・キャンセルと挿入後の通知を両形式で共通に処理します。
ID による `get<N>(id)` は、通常の列では `expected<reference_wrapper<T>, error_code>` を返します。
`record_buffer` では `id`・`get`・`parent`・`index` が行 API 用の予約名です。
`columns()` / `column<N>()` で有効行だけの列ビューも取得できます。
互換性のために公開する `storage.entries` と従来の `get<N>()` では、所有列の長さを直接変更しないでください。
列の再確保などで参照が無効になる操作の後は、取得済みの行参照を取り直してください。

### 列ストレージだけを生成する

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
共有ランナーの速度変動があるため、時間の固定閾値では失敗させず、CSV と生成コードで退行を確認します。
速度はサニタイザなしの実行で比較し、コンパイラ・入力・マシン条件を揃えてください。

## 将来的な機能 (チェックリスト)

- [ ] SIMD 最適化  
- [ ] GPU 並列処理  

---
