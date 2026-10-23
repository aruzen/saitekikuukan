# SaitekiKuukan(最適空間)
> 高性能な C++ 向け Data-Oriented Design (DOD) ライブラリ

`saitekikuukan` は、ゲームエンジンやシミュレーション向けに設計された **データ指向のコンテナライブラリ** です。  
型パラメータパックやメタプログラミングを活用し、SoA / AoSoA / チャンク型データ構造を簡単に扱えます。

---

## 特徴

- **Data-Oriented Design 対応** ✅  
  - チャンク型 (AoSoA) / SoA データ構造で CPU キャッシュ効率を最大化

- **型パック操作・メタプログラミング** ✅  
  - 型リスト (`type_list`) や条件フィルタ (`_filter`) を使った静的型操作  
  - `if constexpr` と `concepts` による安全で可読性の高いコード

- **最新 C++ 機能対応** ✅  
  - 型パック版は C++23、構造体の自動展開は C++26 リフレクションに対応
  - `std::expected`, `constexpr`, `concepts` を活用

- **省メモリ設計** ✅  
  - Stateless 型の EBO に対応  
  - 型リストやチャンク管理で余計なパディングを削減

- **高速化・最適化** 
  - AoSoA / チャンク管理でキャッシュヒット率向上 

- **ヘッダオンリー** ✅  
  - 1 ファイルで完結  
  - 簡単にインクルードして使用可能

---

## インストール

ヘッダオンリーなので、プロジェクトに `include/saitekikuukan.hpp` をコピーしてください！

`add_subdirectory`、またはインストール後の `find_package(saitekikuukan CONFIG REQUIRED)` にも対応します。
通常は `saitekikuukan::saitekikuukan`、構造体を自動展開する場合は `saitekikuukan::reflection` をリンクします。
親プロジェクトの C++ バージョンや最適化フラグは変更しません。

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
`id`・`get`・`parent`・`index` は行 API 用の名前なので、record スキーマのメンバ名には使えません。
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
cmake --build build/reflection --target buffer-test reflection-test
ctest --test-dir build/reflection --output-on-failure
```

---

## 将来的な機能 (チェックリスト)

- [ ] SIMD 最適化  
- [ ] GPU 並列処理  

---
