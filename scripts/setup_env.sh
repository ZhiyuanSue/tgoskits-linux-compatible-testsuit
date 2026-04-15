#!/bin/bash
#
# setup_env.sh - 配置测试环境（Ubuntu/Debian）
#
# 功能:
#   1. 检测并安装 musl-cross 交叉编译工具链
#   2. 检测并安装 QEMU user/system 模拟器
#   3. 检测并安装其他必要依赖（libudev-dev, pkg-config, e2fsprogs 等）
#
# 用法:
#   ./setup_env.sh              # 仅检查
#   ./setup_env.sh --install    # 检查并安装缺失的依赖
#

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }

INSTALL_MODE=false
[[ "$1" == "--install" ]] && INSTALL_MODE=true

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo &>/dev/null && SUDO="sudo" || {
        error "非 root 用户且无 sudo，无法安装软件包"; exit 1
    }
fi

apt_update() { $SUDO apt-get update -qq "$@"; }
apt_install() { $SUDO apt-get install -y -qq "$@"; }

check_command() {
    local cmd="$1" pkg="$2"
    if command -v "$cmd" &>/dev/null; then
        local ver=$("$cmd" --version 2>/dev/null | head -1 || echo "ok")
        info "$cmd ✓ ($ver)"
        return 0
    elif $INSTALL_MODE; then
        warn "$cmd 未找到 (包: $pkg)，将安装"
        return 1
    else
        error "$cmd ✗ (包: $pkg)"
        return 1
    fi
}

echo "=========================================="
echo "  Syscall TestSuit - 环境配置"
echo "=========================================="
echo "  模式: $(if $INSTALL_MODE; then echo '检查+安装'; else echo '仅检查'; fi)"
echo ""

# ===== musl 交叉编译工具链 =====
echo "--- musl 交叉编译工具链 ---"

if $INSTALL_MODE; then
    # 先检查是否已有
    need_musl=false
    for arch in x86_64 riscv64 aarch64 loongarch64; do
        command -v "${arch}-linux-musl-gcc" &>/dev/null || { need_musl=true; break; }
    done

    if $need_musl; then
        apt_update
        apt_install musl-tools 2>/dev/null || true
        info "musl-tools 安装完成"

        # 其他架构: 尝试从 musl.cc 下载
        local musl_url="https://musl.cc"
        for arch in riscv64 aarch64; do
            command -v "${arch}-linux-musl-gcc" &>/dev/null && continue
            local tarball="${arch}-cross.tgz"
            info "下载 $arch musl 交叉工具链..."
            if command -v wget &>/dev/null; then
                wget -q "$musl_url/$tarball" -O "/tmp/$tarball" 2>/dev/null || continue
            elif command -v curl &>/dev/null; then
                curl -sL "$musl_url/$tarball" -o "/tmp/$tarball" 2>/dev/null || continue
            else
                continue
            fi
            info "解压到 /opt/ ..."
            mkdir -p /opt && tar -xzf "/tmp/$tarball" -C /opt/ 2>/dev/null || continue
            rm -f "/tmp/$tarball"
            if [ -d "/opt/${arch}-cross/bin" ]; then
                export PATH="/opt/${arch}-cross/bin:$PATH"
                info "$arch 工具链已安装"
            fi
        done

        # loongarch64 特殊处理
        command -v "loongarch64-linux-musl-gcc" &>/dev/null || \
            warn "loongarch64 工具链需单独安装: https://github.com/LoongsonLab/oscomp-toolchains-for-oskernel"
    else
        info "所有 musl 交叉编译器已存在"
    fi
else
    for arch in x86_64 riscv64 aarch64 loongarch64; do
        check_command "${arch}-linux-musl-gcc" "musl-tools" || true
    done
fi

echo ""

# ===== QEMU =====
echo "--- QEMU ---"
if $INSTALL_MODE; then
    need_qemu=false
    for cmd in qemu-x86_64 qemu-riscv64 qemu-aarch64 qemu-loongarch64 \
               qemu-system-riscv64 qemu-system-x86_64 qemu-system-aarch64; do
        command -v "$cmd" &>/dev/null || { need_qemu=true; break; }
    done
    if $need_qemu; then
        apt_update
        apt_install qemu-user qemu-system-misc 2>/dev/null || true
        info "QEMU 安装完成"
    else
        info "QEMU 已存在"
    fi
else
    for cmd in qemu-x86_64 qemu-riscv64 qemu-aarch64 \
               qemu-system-riscv64 qemu-system-x86_64 qemu-system-aarch64; do
        check_command "$cmd" "qemu-user / qemu-system-misc" || true
    done
fi

echo ""

# ===== 其他依赖 =====
echo "--- 其他依赖 ---"
if $INSTALL_MODE; then
    apt_update
    apt_install libudev-dev pkg-config e2fsprogs build-essential file 2>/dev/null || true
    info "其他依赖安装完成"
else
    check_command "pkg-config" "pkg-config" || true
    check_command "e2fsck" "e2fsprogs" || true
fi

# ===== 最终检查 =====
echo ""
echo "=========================================="
echo "  环境检查报告"
echo "=========================================="
all_ok=true
for entry in "x86_64-linux-musl-gcc: MVP 架构编译" \
             "qemu-x86_64: Linux 参考测试" \
             "qemu-system-riscv64: StarryOS QEMU" \
             "e2fsck: rootfs 处理"; do
    cmd="${entry%%:*}"; desc="${entry##*: }"
    if command -v "$cmd" &>/dev/null; then
        info "$cmd ✓ ($desc)"
    else
        error "$cmd ✗ ($desc)"
        all_ok=false
    fi
done
echo ""
if $all_ok; then
    info "环境就绪！可以运行 ./run_all_tests.sh"
else
    error "环境不完整，请使用 ./setup_env.sh --install 安装缺失的依赖"
    exit 1
fi
