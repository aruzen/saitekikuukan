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
  - C++17/20/26 に対応  
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

## 構造体から名前付き SoA を生成する（C++26 試作）

`saitekikuukan_reflection.hpp` は、構造体の各メンバを同名の列配列に変換します。
配列そのものをメンバとして保持し、`storage.x` と構造化束縛の両方でアクセスできます。

```cpp
#include "saitekikuukan_reflection.hpp"
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
通常のヘッダとは独立しており、GCC 16 では `-std=c++26 -freflection` が必要です。

```sh
cmake -S . -B build/reflection -DCMAKE_CXX_COMPILER=g++-16 \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/reflection --target reflection-test
ctest --test-dir build/reflection --output-on-failure
```

---

## 将来的な機能 (チェックリスト)

- [ ] SIMD 最適化  
- [ ] GPU 並列処理  

---
