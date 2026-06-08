# 平台发布构建：SF32 / MinGW64

本文档记录 `arex-deco-core` 面向固件和 PC 消费方的发布构建流程。
日常本机 CMake 构建只用于开发和测试；正式交付的静态库应通过统一脚本构建、
校验并打包。

## 统一入口

所有平台发布构建都通过同一个脚本执行：

```bash
scripts/build_release.sh --platform sf32
scripts/build_release.sh --platform mingw64
scripts/build_release.sh --platform sf32 --platform mingw64
```

也可以使用逗号或 `all`：

```bash
scripts/build_release.sh --platform sf32,mingw64
scripts/build_release.sh --platform all
```

脚本会按所选平台执行以下步骤：

1. 确认对应 Docker 镜像存在；缺失时自动构建镜像。
2. 清理并重新配置平台专用 CMake 构建目录。
3. 构建 `arex_deco_core` 静态库。
4. 解包 `.a` 静态库并用 `file` 校验内部目标文件格式。
5. 将所有已选平台产物放进同一个发布 tarball。

可选参数：

```bash
scripts/build_release.sh --platform all --rebuild-images
scripts/build_release.sh --platform sf32 --no-image-build
scripts/build_release.sh --platform all --dist-dir /tmp/arex-release
```

- `--rebuild-images`：强制重建所选平台 Docker 镜像。
- `--no-image-build`：镜像不存在时直接失败，不自动构建。
- `--dist-dir`：指定发布目录，默认是 `dist/`。

Docker 镜像 tag 可通过环境变量覆盖：

```bash
AREX_SF32_DOCKER_IMAGE=arex-deco-core:sf32 \
AREX_MINGW_DOCKER_IMAGE=arex-deco-core:mingw \
scripts/build_release.sh --platform all
```

## 发布产物

发布脚本会生成一个 tarball 和对应 SHA-256 校验文件：

```text
dist/arex-deco-core-<api-version>-<platforms>-<git-sha>.tar.gz
dist/arex-deco-core-<api-version>-<platforms>-<git-sha>.tar.gz.sha256
```

如果源码工作区有未提交改动，文件名会带 `-dirty` 后缀，`MANIFEST.txt`
中也会记录 `Source dirty: yes`。

双平台构建时，产物会放在同一个 tarball 中：

```text
arex-deco-core-<api-version>-sf32-mingw64-<git-sha>/
├── MANIFEST.txt
├── include/
│   └── arex_deco/
├── docs/
│   ├── core_api_zh.md
│   ├── platform_release_builds_zh.md
│   └── version_history_zh.md
└── lib/
    ├── mingw64/
    │   ├── artifact-file-report.txt
    │   └── libarex_deco_core.a
    └── sf32/
        ├── artifact-file-report.txt
        └── libarex_deco_core.a
```

`include/arex_deco/` 是公开头文件树。固件或 PC 调用方通常只需要包含：

```c
#include "arex_deco/arex_deco.h"
```

校验 tarball：

```bash
sha256sum -c dist/arex-deco-core-<api-version>-<platforms>-<git-sha>.tar.gz.sha256
```

## SF32 交付

SF32 是 SiFli / 思澈 SF32 芯片族的嵌入式交付目标。普通 quick-start
命令生成的 `build/core/libarex_deco_core.a` 是宿主机产物，Linux 开发机上
通常是 x86-64 静态库，不能作为固件最终库交付。

正式命令：

```bash
scripts/build_release.sh --platform sf32
```

平台细节：

- Dockerfile：`docker/sf32.Dockerfile`
- 默认镜像：`arex-deco-core:sf32`
- 工具清单：`docker/sf32/tools.json`
- CMake toolchain：`cmake/toolchains/sf32lb52x-gcc.cmake`
- 构建目录：`build/sf32/`
- 构建中间产物：`build/sf32/core/libarex_deco_core.a`
- 发布包内路径：`lib/sf32/libarex_deco_core.a`

SF32 Docker 镜像会安装 CMake、Ninja、`file` 和 `arm-none-eabi-gcc`。
toolchain 文件遵循当前固件工程设置：

```text
SF32LB52X
arm-none-eabi-gcc / arm-none-eabi-g++
-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -fno-short-enums
```

`-fno-short-enums` 用于保持 ARM 上公开 C ABI 枚举字段为 4 字节，
与本机 ABI 检查保持一致。

成功构建后，`lib/sf32/artifact-file-report.txt` 应包含类似目标格式：

```text
ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV), not stripped
```

固件集成注意事项：

- 公开 API 是 C-compatible；实现由 C++17 源码构建。
- 固件链接阶段需要使用 SF32 C++ 工具链，或 SDK 支持的 C++ 静态库链接步骤。
- core API 由调用方分配内存，不要求固件提供长期动态分配。
- 固件启动时建议调用 `arex_deco_get_api_version()`，并与发布包
  `MANIFEST.txt` 中的 API version 比对。版本不一致时应尽早中止，
  避免使用过期 ABI 假设运行。

## MinGW64 交付

MinGW64 是 Windows x86_64 / MinGW-w64 消费方使用的 PC 侧产物，
不是 SF32 固件二进制。

正式命令：

```bash
scripts/build_release.sh --platform mingw64
```

平台细节：

- Dockerfile：`docker/mingw.Dockerfile`
- 默认镜像：`arex-deco-core:mingw`
- CMake toolchain：`cmake/toolchains/mingw64-gcc.cmake`
- 构建目录：`build/mingw64/`
- 构建中间产物：`build/mingw64/core/libarex_deco_core.a`
- 发布包内路径：`lib/mingw64/libarex_deco_core.a`

MinGW64 Docker 镜像安装 Ubuntu MinGW-w64 x86_64 posix 编译器、binutils、
CMake、Ninja 和 `file`。toolchain 文件设置：

```text
CMAKE_SYSTEM_NAME=Windows
x86_64-w64-mingw32-gcc / x86_64-w64-mingw32-g++
```

成功构建后，`lib/mingw64/artifact-file-report.txt` 应包含类似目标格式：

```text
Intel amd64 COFF object file
```

Windows / MinGW 集成注意事项：

- 使用同一套公开头文件：`include/arex_deco/`。
- 下游 Windows/MinGW 程序需要用 MinGW C++ 工具链链接，或使用等价的
  C++ 静态库链接步骤。
- 程序启动时建议调用 `arex_deco_get_api_version()`，并与发布包
  `MANIFEST.txt` 中记录的 API version 比对。
