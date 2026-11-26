# AXT-AyaKoto/quickjs-ng-wasi

[QuickJS-NG](https://github.com/quickjs-ng/quickjs)を[WASI SDK](https://github.com/webassembly/wasi-sdk)でWASI(`wasm32-wasip1`)向けにビルドした`qjs.wasm`を提供します。  
ES Module 専用・仮想 FS 向けカスタムモジュールローダー付き。

> [!CAUTION]
> このリポジトリは、`README.md`と`vendor/quickjs`(Submodule)を除きすべてがAIによって実装された、いわゆるVibe-Codingプロダクトです。  
> AIへの指示を行った私(綾坂こと)自身が使用しているためある程度品質については保証しますが、コードベースのほとんどをAIエージェントに委任しているプロダクトであることには留意してください。  
> なお、使用ツール・モデルは以下のとおりです。
>
> - Cursor: Grok 4.7

## License

copyright (c) 2026- Ayasaka-Koto.  
Released under the [MIT License](LICENSE).

> [!NOTE]
> submodule [`quickjs-ng/quickjs`](https://github.com/quickjs-ng/quickjs)のライセンスもMIT Licenseです。  
> 詳細は[LICENSE](https://github.com/quickjs-ng/quickjs/blob/master/LICENSE)をご確認ください。

## Usage / Behavior

### CLI

```sh
qjs.wasm entrypoint.js
qjs.wasm --shim-dir shim-directory entrypoint.js
```

`--shim-dir`の既定値は`/shims`です。

### Module Resolution

- `qjs:os` → [QuickJS-NG Standard Library `qjs:os` module](https://quickjs-ng.github.io/quickjs/stdlib/#qjsos-module)
- `qjs:std` → [QuickJS-NG Standard Library `qjs:std` module](https://quickjs-ng.github.io/quickjs/stdlib/#qjsstd-module)
- `node:<name>` → `<shim-dir>/node/<name>.js`
- bare specifier `<name>` → `<shim-dir>/bare/<name>.js`
- other (`./`, `/`) → そのとおり相対・絶対パスとして解釈

### Preload

`<shim-dir>/preload.js`が存在すれば`entrypoint.js`より先に評価します。

### Stack Size

諸事情で少し引き上げてあります。

- WASM側: 128MiB
- QuickJS側: 120MiB

### How to build

事前に[WASI SDK](https://github.com/webassembly/wasi-sdk)(`wasi-sdk`)を当該GitHub Releasesからダウンロードして展開してください。  
WASI SDKのパスを`/path/to/wasi-sdk`として、以下のコマンドで手動ビルドできます。

```sh
git submodule update --init
WASI_SDK_PATH=/path/to/wasi-sdk ./scripts/build.sh
```

ビルド成果物は`dist/qjs.wasm`, `dist/qjs.wasm.sha256`の2ファイルです。

> [!NOTE]
> バージョンごとのビルド成果物は[GitHub Releases](https://github.com/AXT-AyaKoto/quickjs-ng-wasi/releases)からダウンロードすることもできます。