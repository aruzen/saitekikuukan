# SaitekiKuukan（最適空間）

C++23のヘッダオンリーSoAコンテナです。属性ごとの連続配列を提供し、ID・イベント・GPU APIを使わずに利用できます。

現在の実用化対象は `DynamicColumns` と `SoA<Row>` です。AoS/AoSoAは未実装です。従来の `buffer` は開発中で、今回修復した基本操作以外のイベント・UUID・coroutine generator等は対象外です。

## 使い方

```cpp
#include <saitekikuukan/soa.hpp>
#include <array>

struct Vertex {
    std::array<float, 3> normal{0, 0, 1};
    std::array<float, 2> uv0{}, uv1{};
};

template<>
struct kk::Schema<Vertex> {
    using Members = kk::MemberList<&Vertex::normal, &Vertex::uv0, &Vertex::uv1>;
};

int main() {
    kk::SoA<Vertex> vertices(3);
    auto uvs = vertices.column<&Vertex::uv0>();
    uvs[0] = {0, 0};
    uvs[1] = {1, 0};
    uvs[2] = {0, 1};

    auto row = vertices.at(1);
    row.get<&Vertex::uv1>() = {0.5f, 0.5f};
    Vertex value = row.read();
    vertices.at(2).write(value);
}
```

UV0とUV1が同じ型でもメンバーポインターで区別します。Vector/array等は1属性1列として保持し、成分ごとの自動分割は行いません。schemaに登録するメンバーは行型自身のデータメンバーで、重複は拒否します。登録していないメンバーは保存されず、read時にはRowの既定値になります。

## 契約

- SoAの行型はdefault/copy constructible、登録する属性は書き戻し可能な型を使います。各列はcopy constructibleまたはnothrow move constructibleである必要があります。`vector<bool>`を避けるため、bool属性は非対応です。必要ならuint8_t等を使ってください。
- `size/empty/capacity/reserve/resize/push_back/pop_back/clear`を提供します。resizeによる追加は`Row{}`のメンバー初期値を使います。空schemaでも行数を保持します。
- `column<Member>()`はactive size分のspan、const版はspan<const T>です。所有配列のサイズを直接変更するAPIは公開しません。
- `at/read/write`は境界を検証し、範囲外ではout_of_rangeを送出します。`operator[]`とRowRefのgetは範囲内を前提にします。
- RowRefは親コンテナと添字を保持します。readは構造体へコピー、writeは各列への代入です。Row&は返しません。proxy同士の代入は曖昧さを避けるため禁止し、明示的にwrite/readします。
- SoAの行iteratorはまだ提供しません。処理する属性のspanをループしてください。従来bufferのiteratorは参照proxyを返し、行の並べ替え可能性は保証しません。
- 追加・resizeの失敗時も全列の長さを揃えます。copy assignmentは一時コピーとswapを使います。writeや個々の要素の代入が例外を投げた場合、変更済みの属性値までは戻しません。
- reserve/追加/拡張でspan・要素参照が無効になる場合があります。失敗したreserveでも一部の列の再確保は完了している可能性があります。縮小・clearは削除した要素の参照を無効にします。
- RowRefは再確保後も同じ親/添字を指しますが、要素削除後の範囲外参照、親の破棄・move・swapをまたぐ使用は避けてください。
- SoA自体はCPUコンテナです。GPU向けのformat・stride・転送・同期は呼び出し側が担当します。SIMD化や速度向上は保証せず、実際の処理で測定してください。

## 組み込みとテスト

```cmake
add_subdirectory(path/to/saitekikuukan)
target_link_libraries(app PRIVATE saitekikuukan::saitekikuukan)
```

install後は`find_package(saitekikuukan CONFIG REQUIRED)`でも利用できます。通常利用にBoost等の依存はありません。テストを有効にする場合だけGTestを使います。

```sh
cmake -S . -B build -DSAITEKIKUUKAN_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

親projectから追加した場合はテストを既定で無効にします。開発用benchmarkは`SAITEKIKUUKAN_BUILD_BENCHMARKS=ON`でビルドできます。親projectのC++規格や最適化フラグは変更しません。

## 従来bufferからの変更

- 可変長storageとSoAは同じDynamicColumnsを使用します。ID付きstorageではIDを先頭列に持ち、属性列と一括で更新します。
- `buffer.get<N>()`は可変vectorではなくspanを返します。動的storageの公開`entries/ids`は廃止し、`get<N>()/id_at()`で参照します。
- `buffer.get<N>(id)`は`expected<reference_wrapper<T>, error_code>`です。取得後はvalue().get()で要素へアクセスします。
- ID=0は有効です。bufferのresizeは追加分のIDを生成し、eraseは末尾との交換削除です。Meshの頂点indexとしてIDを流用しないでください。
- 固定長storageはarray全容量分の要素を所有し、clear/pop_backは論理サイズだけを変えます。
- 未完成のイベント発火は利用できないことをcompile時に診断します。従来の型パック全機能やgeneratorの実用化は今後の対象です。
