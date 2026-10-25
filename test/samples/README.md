# 実行できる使用例

各 `.cpp` は独立した `main` を持ち、標準入力なしで実行できます。
用途ごとの操作結果を `check.hpp` で確認し、成功時に短い結果を表示します。
結果の確認は Release ビルドでも有効です。

| ソース | 標準 | 使用例 |
| --- | --- | --- |
| [particles.cpp](particles.cpp) | C++23 | 型パックのSoA、構造化束縛での列・行更新、値のread/write |
| [fixed-capacity.cpp](fixed-capacity.cpp) | C++23 | 固定容量、容量超過の処理、末尾行を移す削除、const列ビュー |
| [buffer-ids.cpp](buffer-ids.cpp) | C++23 | IDと物理インデックスの違い、ID検索、expectedによる失敗処理 |
| [insertion-events.cpp](insertion-events.cpp) | C++23 | リスナの登録・解除、挿入前の値変更とキャンセル、挿入後通知 |
| [chunked-particles.cpp](chunked-particles.cpp) | C++23 | 128行ずつのChunk、半端な末尾の走査、reserve後の参照、境界をまたぐ削除 |
| [custom-columns.cpp](custom-columns.cpp) | C++23 | dequeを列にする独自layoutとrangesによる行の絞り込み |
| [move-only.cpp](move-only.cpp) | C++23 | unique_ptrの列、コンテナのムーブ、所有権を保った行更新 |
| [strategy-selection.cpp](strategy-selection.cpp) | C++23 | 同じ配置でIDなし・ハッシュ索引・線形検索を選択 |
| [reflected-particles.cpp](reflected-particles.cpp) | C++26 | 構造体の初期値、storage.x、it->x、メンバ指定の列、read/write |
| [reflected-chunks.cpp](reflected-chunks.cpp) | C++26 | Chunk内の名前付き列、境界をまたぐ名前付き行、追加後の参照 |
| [reflected-buffer.cpp](reflected-buffer.cpp) | C++26 | bufferでのID検索と名前付きメンバへのアクセス |
| [raw-columns.cpp](raw-columns.cpp) | C++26 | 所有列の直接操作、全列への処理、最短の列長に合わせる行ビュー |

リポジトリのルートから、C++23版をビルド・実行できます。

```sh
cmake -S . -B build/samples -DCMAKE_CXX_COMPILER=clang++
cmake --build build/samples --parallel 3
ctest --test-dir build/samples -L sample --output-on-failure
```

個別に実行する場合は、ファイル名に対応した `sample-名前` ターゲットを使います。

```sh
cmake --build build/samples --target sample-particles
./build/samples/test/samples/sample-particles
```

reflection版は、対応するGCCでテストを有効にします。
`saitekikuukan::reflection` ターゲットからC++26と `-freflection` が設定されます。

```sh
cmake -S . -B build/samples-reflection -DCMAKE_CXX_COMPILER=g++ \
  -DSAITEKIKUUKAN_BUILD_REFLECTION_TESTS=ON
cmake --build build/samples-reflection --parallel 3
ctest --test-dir build/samples-reflection -L sample --output-on-failure
```

通常版8件は `SAITEKIKUUKAN_BUILD_TESTS`、reflection版4件は
`SAITEKIKUUKAN_BUILD_REFLECTION_TESTS` に連動します。既存のテストも一緒に実行する場合は
CTestの `-L sample` を外してください。

列ビューや行プロキシを保持したままサイズを変更せず、変更後はビューを取り直してください。
Chunk内の列は連続していますが、全体の列はチャンクをまたぐ範囲です。
リスナの登録先はリスナを所有しないため、登録中のリスナの寿命は利用側で管理します。
