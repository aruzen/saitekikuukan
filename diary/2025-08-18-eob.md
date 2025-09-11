## `[[no_unique_address]]`で実行時にはメモリを取らなくできた
### 2025-08-18
- `空クラス(EmptyClass)`メンバも仮想関数も持たないクラス

C++の使用上、空クラスも実体化すると固有のアドレスを
与えなければいけない関係上1Byte分メモリを取ります、この仕様は
```c++
struct Runner2 {
    inline static void run();
};

struct Runner1 {
    int num;
    void run();
};

template<typename Runner>
struct Nanika{
    Runner runner;
    std::string name;
    
    void run() {
        runner.run();
    }
};
```

の時`Nanika<Runner2>`は`sizeof(Runner2)==4`の分ちゃんと取られます、
では`Nanika<Runner1>`はどうでしょうこの場合`sizeof(Runner1)==1`の分取られてしまいます、メンバーがないのに。

そこででてくるのが`[[no_unique_address]]`ですこれをつけると
`Runner2`の分のメモリは取られず、メモリの配置上は`Nanika<Runner1> n`の時`&n.runnner == &n.name`になります。

今回はところどころ内部変数がいらないものがあったのでそれに`[[no_unique_address]]`をつけることで
```
sizeof(static_buffer<generator::uuid, eventsystem::dummy, 4, int, int>) : 104
sizeof(size_t) + sizeof(boost::uuids::uuid)*4 + sizeof(int)*(4+4) : 104
```
を実現できました知識としては知っていましたがちゃんと確認したのは初めてです。
