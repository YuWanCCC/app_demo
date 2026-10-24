# app_demo — RV1126B App 独立工程

从 `rv1268B` SDK 的 `project/app` 移植而来的独立工程，完全自包含，
可脱离原 SDK 编译（工具链、预编译库均随工程携带）。

## 目录结构

```
app_demo/
├── build.sh              # 构建入口
├── env.sh                # 环境配置（板卡参数 + 依赖路径，可移植性的关键）
├── app/                  # 应用源码（rkipc/rkai/uvc_app_tiny/wifi_app/nginx 等）
├── toolchain/            # aarch64-rockchip1240-linux-gnu 交叉工具链（自带）
├── media_out/            # media 预编译库（include + lib + root，自带）
└── third_party/
    ├── toolkits/         # nginx 依赖的 openssl/zlib 预编译库（自带）
    └── pcre2/            # pcre2 源码包（自带）
```

## 快速开始

```bash
./build.sh            # 编译 RK_APP_TYPE 使能的所有应用
./build.sh clean      # 清理
./build.sh info       # 查看当前构建配置
```

产物输出在 `app/out/`。

## 移植到其它机器

本工程完全自包含（工具链、预编译库均随工程携带，总计约 1.9G），
把整个 `app_demo/` 目录拷贝到任何 Linux 机器即可直接 `./build.sh`，
无需安装或配置任何外部依赖。

如需改用外部工具链（如机器上已有），修改 `env.sh` 中的 `RK_TOOLCHAIN_DIR`
即可，目录布局要求：

```
$RK_TOOLCHAIN_DIR/aarch64-rockchip1240-linux-gnu/bin/aarch64-rockchip1240-linux-gnu-gcc
```

工具链也可不通过 `RK_TOOLCHAIN_DIR` 提供：只要 `aarch64-rockchip1240-linux-gnu-gcc`
在 `PATH` 中，构建系统会自动回退使用 PATH 中的工具链。

## 常用配置（均在 env.sh）

| 变量 | 说明 | 默认值 |
| --- | --- | --- |
| `RK_APP_TYPE` | 选择编译哪些应用 | `RKIPC_RV1126B RK_APP_RKAI UVC_TINY` |
| `RK_BUILD_VERSION_TYPE` | RELEASE / DEBUG / DEBUG_ASAN | `RELEASE` |
| `RK_JOBS` | 并行编译数 | 核数/2+1 |
| `RK_TOOLCHAIN_DIR` | 工具链根目录 | 工程内 `toolchain` |
| `RK_PROJECT_PATH_MEDIA` | media 预编译库 | 工程内 `media_out` |

`RK_APP_TYPE` 常用取值：

- `RKIPC_RV1126B` / `RKIPC_RV1126B_DUAL_IPC` / `RKIPC_RV1126B_DV` → rkipc
- `RK_APP_RKAI` → rkai（RKAI 框架）
- `UVC_TINY` → uvc_app_tiny
- `RK_WIFI_APP` → wifi_app（wpa_supplicant/bluez 等）
- `YOLOWORLD_DEMO` / `PROJECTOR_DEMO` → testdemo

## 只编译单个应用

```bash
source env.sh
export RK_PROJECT_OUTPUT=$(pwd)/app     # 触发独立编译模式
make -C app/rkipc                       # 只编 rkipc
make -C app/uvc_app_tiny                # 只编 uvc_app_tiny
```

## 与原 SDK 的差异

1. `app/Makefile.param`：工具链路径改为可由 `RK_TOOLCHAIN_DIR` 覆盖，
   并增加 `RK_PROJECT_PATH_MEDIA` 存在性检查（不再强依赖
   `../../.BoardConfig.mk` 与 `../../output/out`）。
2. `app/nginx_mk`、`app/component/nginx_mk`：openssl/zlib/pcre2 路径改为
   由 `RK_APP_TOOLKITS_DIR`、`RK_APP_PCRE2_TAR_DIR` 环境变量提供，
   默认值仍兼容原 SDK 内编译。
3. 板卡参数（`RK_CHIP`/`RK_APP_TYPE` 等）原由 `.BoardConfig.mk` 提供，
   现集中到 `env.sh`。
