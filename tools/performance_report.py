#!/usr/bin/env python3
"""同じ数値処理を -O2/-O3 で計測し、CSV・生成コード・ベクトル化診断を保存する。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


def run(command, log):
    result = subprocess.run(command, text=True, capture_output=True)
    log.write_text(result.stderr)
    if result.returncode:
        raise SystemExit(f"Failed ({result.returncode}): {command}\n{result.stderr}")
    return result.stdout


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--reflection", action="store_true", help="GCC 16以降のreflectionも計測")
    parser.add_argument("--include-dir", type=Path, default=root / "include", help="比較する版のヘッダディレクトリ")
    parser.add_argument("--output", type=Path, default=root / "build/performance")
    parser.add_argument("--levels", nargs="+", choices=["O2", "O3"], default=["O2", "O3"])
    parser.add_argument("--rows", type=int, default=262144)
    parser.add_argument("--iterations", type=int, default=24)
    parser.add_argument("--samples", type=int, default=7)
    args = parser.parse_args()
    if not (1 <= args.rows <= 2**24 and 1 <= args.iterations <= 1000 and 1 <= args.samples <= 1000):
        parser.error("rows=1..2^24、iterations/samples=1..1000を指定してください")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = run([args.compiler, "--version"], output / "version-stderr.txt")
    clang = "clang" in compiler.lower()
    if not clang and "Free Software Foundation" not in compiler:
        parser.error("GCCまたはClangを指定してください")
    if args.reflection and clang:
        parser.error("現在のreflection計測はGCC 16以降を対象としています")
    include = args.include_dir.resolve()
    header = include / "saitekikuukan.hpp"
    if not header.is_file():
        parser.error(f"ヘッダが見つかりません: {header}")
    sources = root / "test/performance"
    standard = ["-std=c++26", "-freflection"] if args.reflection else ["-std=c++23"]
    metadata = {"compiler": compiler, "platform": platform.platform(),
                "target": run([args.compiler, "-dumpmachine"], output / "target-stderr.txt").strip(),
                "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
                "include_dir": str(include), "rows": args.rows, "iterations": args.iterations,
                "samples": args.samples, "reflection": args.reflection,
                "sources_sha256": {name: hashlib.sha256((sources / name).read_bytes()).hexdigest()
                                   for name in ["types.hpp", "kernels.cpp", "benchmark.cpp"]}, "commands": []}
    for level in args.levels:
        flags = [args.compiler, *standard, "-" + level, "-DNDEBUG", "-Wall", "-Wextra",
                 "-pedantic-errors", "-I", str(include)]
        diagnostics = ["-Rpass=loop-vectorize", "-Rpass-missed=loop-vectorize"] if clang else [
            "-fopt-info-vec-optimized", "-fopt-info-vec-missed"]
        assembly = [*flags, *diagnostics, "-S", str(sources / "kernels.cpp"), "-o", str(output / f"{level}.s")]
        binary = output / ("benchmark-" + level)
        build = [*flags, str(sources / "kernels.cpp"), str(sources / "benchmark.cpp"), "-o", str(binary)]
        execute = [str(binary), "--rows", str(args.rows), "--iterations", str(args.iterations),
                   "--samples", str(args.samples)]
        metadata["commands"].extend([assembly, build, execute])
        (output / "metadata.json").write_text(json.dumps(metadata, indent=2))
        run(assembly, output / f"{level}-vectorization.txt")
        run(build, output / f"{level}-build.txt")
        result = run(execute, output / f"{level}-run.txt")
        (output / f"{level}.csv").write_text(result)
        print(f"{level}:\n{result}", end="")
    print(f"Reports: {output}")


if __name__ == "__main__":
    main()
