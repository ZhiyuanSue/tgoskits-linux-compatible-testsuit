# StarryOS 全系统调用测试优先级排序

## 评估方法

每个 syscall 组在 4 个维度打分（1-5 分），加权求和：

| 维度 | 权重 | 含义 | 评分标准 |
|------|------|------|----------|
| 依赖度 (D) | 30% | 多少其他测试依赖它 | 5=几乎所有测试需要 → 1=完全独立 |
| 频次 (F) | 15% | 真实应用中的调用频率 | 5=极高频 → 1=极少使用 |
| 杀伤力 (S) | 35% | 出 bug 后的影响范围 | 5=内核崩溃/安全漏洞 → 1=功能退化 |
| 复杂度 (C) | 20% | 代码路径数、kai状态空间大小 | 5=大量状态/并发 → 1=简单读写 |

**加权总分 = 0.3D + 0.15F + 0.35S + 0.2C**，范围 [1.0, 5.0]

**调整说明**: 相比原版本，杀伤力权重从 30% 提升至 35%，频次从 20% 降至 15%。

---

## 一、StarryOS 已实现系统调用全清单（211 个 Sysno 条目）

按源码 `kernel/src/syscall/mod.rs` 中的分类整理：

### 1. 文件系统控制 (fs ctl) — 30 个

| syscall | 说明 |
|---------|------|
| ioctl | 设备控制 |
| chdir / fchdir / chroot | 工作目录切换 |
| mkdir / mkdirat | 创建目录 |
| getdents64 | 读取目录项 |
| link / linkat | 创建硬链接 |
| rmdir | 删除空目录 |
| unlink / unlinkat | 删除文件/目录 |
| getcwd | 获取当前工作目录 |
| symlink / symlinkat | 创建符号链接 |
| rename / renameat / renameat2 | 重命名 |
| sync / syncfs | 同步文件系统 |
| readlink / readlinkat | 读取符号链接 |
| utime / utimes / utimensat | 修改时间戳 |

### 2. 文件权限 (file ownership) — 14 个

| syscall | 说明 |
|---------|------|
| chown / lchown / fchown / fchownat | 修改所有者 |
| chmod / fchmod / fchmodat / fchmodat2 | 修改权限 |
| access / faccessat / faccessat2 | 检查权限 |
| umask | 设置文件模式创建屏蔽字 |
| statfs / fstatfs | 文件系统统计 |

### 3. 文件描述符操作 (fd ops) — 10 个

| syscall | 说明 |
|---------|------|
| open / openat | 打开文件 |
| close / close_range | 关闭文件 |
| dup / dup2 / dup3 | 复制 fd |
| fcntl | fd 控制 |
| flock | 文件锁 |

### 4. 基本 IO — 16 个

| syscall | 说明 |
|---------|------|
| read / write | 基本读写 |
| readv / writev | 分散/聚集 IO |
| lseek | 文件偏移 |
| pread64 / pwrite64 | 定位读写 |
| preadv / pwritev / preadv2 / pwritev2 | 定位分散/聚集 |
| truncate / ftruncate | 截断文件 |
| fallocate | 预分配空间 |
| fsync / fdatasync | 同步文件 |
| fadvise64 | 文件访问建议 |

### 5. 高级 IO — 4 个

| syscall | 说明 |
|---------|------|
| sendfile | 零拷贝传输 |
| copy_file_range | 文件间复制 |
| splice | 管道拼接 |

### 6. IO 多路复用 — 10 个

| syscall | 说明 |
|---------|------|
| poll / ppoll | poll 系列 |
| select / pselect6 | select 系列 |
| epoll_create1 / epoll_ctl / epoll_pwait / epoll_pwait2 | epoll 系列 |

### 7. 文件元数据 (fs stat) — 9 个

| syscall | 说明 |
|---------|------|
| stat / lstat / fstat / fstatat(newfstatat) | 文件信息 |
| statx | 扩展文件信息 |

### 8. 内存管理 (mm) — 12 个

| syscall | 说明 |
|---------|------|
| brk | 堆内存调整 |
| mmap / munmap / mprotect | 内存映射 |
| mremap | 重映射 |
| mincore | 页面驻留检查 |
| madvise / msync | 内存建议/同步 |
| mlock / mlock2 | 内存锁定 |

### 9. 进程信息 (task info) — 5 个

| syscall | 说明 |
|---------|------|
| getpid / getppid / gettid | 获取进程/线程 ID |
| getrusage | 获取资源使用 |

### 10. 进程调度 (task sched) — 10 个

| syscall | 说明 |
|---------|------|
| sched_yield | 让出 CPU |
| nanosleep / clock_nanosleep | 睡眠 |
| sched_get/setaffinity | CPU 亲和性 |
| sched_get/set scheduler | 调度策略 |
| sched_getparam | 调度参数 |
| getpriority | 优先级 |

### 11. 进程操作 (task ops) — 16 个

| syscall | 说明 |
|---------|------|
| clone / clone3 / fork | 创建进程/线程 |
| execve | 执行程序 |
| exit / exit_group | 退出 |
| wait4 | 等待子进程 |
| setsid / getsid | 会话管理 |
| setpgid / getpgid | 进程组管理 |
| set_tid_address | 线程 ID 地址 |
| arch_prctl | 架构相关控制 |
| prctl | 进程控制 |
| prlimit64 | 资源限制 |
| capget / capset | 能力管理 |
| get_mempolicy | 内存策略 |
| setreuid / setresuid / setresgid | 用户/组切换 |

### 12. 信号 (signal) — 15 个

| syscall | 说明 |
|---------|------|
| rt_sigaction | 设置信号处理 |
| rt_sigprocmask | 信号屏蔽 |
| rt_sigpending | 待处理信号 |
| rt_sigreturn | 信号返回 |
| rt_sigsuspend | 原子等待信号 |
| rt_sigtimedwait | 定时等待信号 |
| kill / tkill / tgkill | 发送信号 |
| rt_sigqueueinfo / rt_tgsigqueueinfo | 信号队列 |
| sigaltstack | 备用信号栈 |

### 13. 同步 (sync) — 4 个

| syscall | 说明 |
|---------|------|
| futex | 快速用户空间锁 |
| get_robust_list / set_robust_list | robust futex |
| membarrier | 内存屏障 |

### 14. 系统 (sys) — 13 个

| syscall | 说明 |
|---------|------|
| getuid / geteuid / getgid / getegid | 用户/组 ID |
| setuid / setgid | 设置用户/组 |
| getgroups / setgroups | 组列表 |
| uname | 系统信息 |
| sysinfo | 系统统计 |
| syslog | 系统日志 |
| getrandom | 随机数 |
| seccomp | 安全计算 |

### 15. 时间 (time) — 6 个

| syscall | 说明 |
|---------|------|
| gettimeofday | 获取时间 |
| times | 进程时间 |
| clock_gettime / clock_getres | 时钟 |
| getitimer / setitimer | 定时器 |
| timer_create / timer_gettime / timer_settime | POSIX 定时器(桩) |

### 16. System V 消息队列 — 4 个

| syscall | 说明 |
|---------|------|
| msgget / msgsnd / msgrcv / msgctl | 消息队列 |

### 17. System V 共享内存 — 4 个

| syscall | 说明 |
|---------|------|
| shmget / shmat / shmctl / shmdt | 共享内存 |

### 18. 网络 (net) — 20 个

| syscall | 说明 |
|---------|------|
| socket / socketpair | 创建套接字 |
| bind / connect / listen / accept / accept4 | 连接管理 |
| getsockname / getpeername | 地址获取 |
| sendto / recvfrom | 数据收发 |
| sendmsg / recvmsg | 消息收发 |
| getsockopt / setsockopt | 套接字选项 |
| shutdown | 关闭连接 |

### 19. 特殊 fd — 6 个

| syscall | 说明 |
|---------|------|
| pipe / pipe2 | 管道 |
| eventfd2 | 事件 fd |
| signalfd4 | 信号 fd |
| memfd_create | 匿名文件 |
| pidfd_open / pidfd_getfd / pidfd_send_signal | 进程 fd |

### 20. 文件系统挂载 — 2 个

| syscall | 说明 |
|---------|------|
| mount / umount2 | 挂载/卸载 |

### 21. Dummy (返回占位 fd) — 10 个

timerfd_create, fanotify_init, inotify_init1, userfaultfd, perf_event_open, io_uring_setup, bpf, fsopen, fspick, open_tree, memfd_secret

---

## 二、逐组评分（修正版）

> 211 个 Sysno 条目按功能聚合为 **30 个测试组**，逐一评分。
> 加权总分 = 0.3D + 0.15F + 0.35S + 0.2C

| # | 测试组 | 包含 syscall | D | F | S | C | 总分 | 备注 |
|---|--------|-------------|---|---|---|---|------|------|
| 1 | 基本 IO | read/write/lseek/close | 5 | 5 | 5 | 2 | **4.40** | 所有测试的基础 |
| 2 | openat | openat/open | 5 | 5 | 4 | 3 | **4.20** | 文件入口 |
| 3 | fork/wait | fork/clone/clone3/wait4/exit/execve | 4 | 3 | 5 | 5 | **4.25** | 进程生命周期 |
| 4 | dup/fcntl | dup/dup2/dup3/fcntl/flock | 4 | 4 | 3 | 4 | **3.70** | fd 生命周期 |
| 5 | 信号全套 | rt_sigaction/procmask/pending/kill/tgkill | 4 | 2 | 5 | 5 | **4.05** | 异常处理 |
| 6 | mmap | mmap/munmap/mprotect/mremap | 3 | 4 | 5 | 4 | **4.00** | 内存安全 |
| 7 | brk | brk | 3 | 4 | 4 | 2 | **3.35** | malloc 依赖 |
| 8 | futex | futex | 3 | 2 | 4 | 5 | **3.50** | 锁原语 |
| 9 | stat | stat/lstat/fstat/fstatat/statx | 3 | 4 | 2 | 2 | **2.65** | 观测手段 |
| 10 | socket 全套 | socket/bind/listen/accept4/connect | 2 | 3 | 3 | 5 | **3.10** | 网络 TCP/UDP |
| 11 | epoll | epoll_create1/ctl/pwait | 2 | 3 | 3 | 4 | **2.95** | 事件驱动 |
| 12 | 目录操作 | mkdirat/getcwd/chdir/unlinkat/rename | 3 | 3 | 3 | 3 | **3.00** | 路径管理 |
| 13 | pipe2 | pipe/pipe2 | 3 | 3 | 2 | 2 | **2.50** | IPC 基础 |
| 14 | 时间 | clock_gettime/getres/gettimeofday/nanosleep | 3 | 3 | 2 | 2 | **2.50** | 时间基础 |
| 15 | readv/writev | readv/writev/pread64/pwritev | 2 | 2 | 2 | 2 | **2.00** | IO 变体 |
| 16 | socket IO | sendto/recvfrom/sendmsg/recvmsg | 2 | 3 | 2 | 3 | **2.45** | 附属 socket |
| 17 | 高级 IO | sendfile/copy_file_range/splice | 2 | 2 | 3 | 3 | **2.50** | IO 优化 |
| 18 | 信号扩展 | sigaltstack/sigsuspend/sigtimedwait | 3 | 1 | 4 | 4 | **3.10** | 附属信号 |
| 19 | select/poll | select/pselect6/poll/ppoll | 2 | 3 | 2 | 3 | **2.45** | 传统多路复用 |
| 20 | 文件权限 | chmod/fchmod/chown/fchown/faccessat | 2 | 2 | 3 | 2 | **2.30** | 权限语义 |
| 21 | mincore/madvise | mincore/madvise/msync/mlock | 2 | 2 | 3 | 2 | **2.30** | 附属 mmap |
| 22 | 定时器 | getitimer/setitimer/timer_create | 2 | 2 | 3 | 3 | **2.50** | 附属时间 |
| 23 | 会话/进程组 | setsid/getsid/setpgid/getpgid | 2 | 2 | 2 | 2 | **2.00** | 附属 fork |
| 24 | socket 选项 | getsockname/getpeername/getsockopt/setsockopt/shutdown | 2 | 2 | 2 | 2 | **2.00** | 附属 socket |
| 25 | socketpair | socketpair | 2 | 2 | 2 | 2 | **2.00** | 附属 socket |
| 26 | 共享内存 | shmget/shmat/shmctl/shmdt | 2 | 1 | 3 | 2 | **2.05** | System V |
| 27 | 消息队列 | msgget/msgsnd/msgrcv/msgctl | 1 | 1 | 2 | 3 | **1.70** | System V |
| 28 | 特殊 fd | eventfd2/signalfd4 | 1 | 1 | 1 | 2 | **1.20** | 低优先级 |
| 29 | 现代 fd | memfd_create/pidfd_open/pidfd_getfd | 1 | 1 | 2 | 2 | **1.50** | Linux 5.x 新特性 |
| 30 | 系统信息 | uname/sysinfo/getuid/setuid/getrandom | 2 | 2 | 2 | 1 | **1.85** | 简单查询 |

---

## 三、最终测试顺序（按优先级分层，修正版）

### Tier 1: 基础设施层 — 其他所有测试都依赖（总分 3.7+）

> 不通过这些，后续测试全部无法运行

| 顺序 | 测试组 | 总分 | 包含 syscall | 理由 |
|------|--------|------|-------------|------|
| T1-1 | **基本 IO** | 4.40 | read/write/lseek/close | 测试框架自身 printf 依赖它；最基本的数据通路 |
| T1-2 | **openat** | 4.20 | openat/open | 文件操作的唯一入口；stat/目录等测试都需要先打开文件 |
| T1-3 | **fork/wait** | 4.25 | fork/clone/clone3/wait4/execve/exit | 进程生命周期；多进程/并发基础；fork 错误 = 整个多进程模型失效 |
| T1-4 | **dup/fcntl** | 3.70 | dup/dup2/dup3/fcntl/flock | fd 管理是 pipe/socket/epoll 的基础；已发现 F_DUPFD bug |
| T1-5 | **信号全套** | 4.05 | rt_sigaction/rt_sigprocmask/kill/tgkill/rt_sigpending | 信号链路错误 = SIGSEGV 无法处理 = 进程无故死亡 |
| T1-6 | **mmap** | 4.00 | mmap/munmap/mprotect/mremap | 内存保护错误 = 任意读写 = 安全漏洞；已发现 mprotect bug |

**修正说明**:

- 将 fork/wait 提升到 T1，因为多进程测试是核心
- 将信号全套提升到 T1，因为信号处理失败 = 崩溃
- 将 mmap 提升到 T1，因为内存安全是最高优先级
- stat 降级到 T2，因为虽然重要但不是阻塞依赖

### Tier 2: 核心组件层 — 高频使用/关键功能（总分 3.0-3.7）

> 这些 syscall 构成完整的应用运行环境

| 顺序 | 测试组 | 总分 | 包含 syscall | 理由 |
|------|--------|------|-------------|------|
| T2-1 | **brk** | 3.35 | brk | malloc/free 的基础；错误 = 堆损坏 |
| T2-2 | **futex** | 3.50 | futex | 所有用户态锁的基础；错误 = 死锁/数据竞争 |
| T2-3 | **stat** | 2.65 | stat/lstat/fstat/fstatat/statx | 作为独立观测手段被大量测试使用（验证文件大小/权限/类型） |
| T2-4 | **socket 全套** | 3.10 | socket/bind/listen/accept4/connect | TCP/UDP 通信基础；已发现 accept4 返回地址 bug |
| T2-5 | **epoll** | 2.95 | epoll_create1/epoll_ctl/epoll_pwait | 事件驱动核心；nginx/Redis 类应用依赖 |
| T2-6 | **目录操作** | 3.00 | mkdirat/getcwd/chdir/unlinkat/rename/linkat/symlinkat/getdents64 | 路径管理；文件系统基础操作 |
| T2-7 | **信号扩展** | 3.10 | sigaltstack/rt_sigsuspend/rt_sigtimedwait | 高级信号处理；栈溢出保护 |

**修正说明**:

- 新增目录操作到 T2，评分 3.0 符合 T2 标准
- stat 虽然评分 2.65，但因作为"观测手段"被大量依赖，保留在 T2
- 信号扩展评分 3.10，位置正确

### Tier 3: 通信与辅助功能（总分 2.3-2.5）

> 构成完整的应用通信能力

| 顺序 | 测试组 | 总分 | 包含 syscall | 理由 |
|------|--------|------|-------------|------|
| T3-1 | **pipe2** | 2.50 | pipe/pipe2 | 最简 IPC；fork 测试中大量使用；测试 O_NONBLOCK 的基础 |
| T3-2 | **时间** | 2.50 | clock_gettime/clock_getres/gettimeofday/nanosleep/times | 时间基础；超时/定时依赖 |
| T3-3 | **高级 IO** | 2.50 | sendfile/copy_file_range/splice | IO 优化；零拷贝传输 |
| T3-4 | **定时器** | 2.50 | getitimer/setitimer/timer_create | 精确定时；SIGALRM 处理 |
| T3-5 | **socket IO** | 2.45 | sendto/recvfrom/sendmsg/recvmsg | 数据收发；大数据传输验证 |
| T3-6 | **select/poll** | 2.45 | select/pselect6/poll/ppoll | 传统多路复用；兼容性测试 |
| T3-7 | **文件权限** | 2.30 | chmod/fchmod/chown/fchown/faccessat/umask | 权限语义；安全基础 |
| T3-8 | **mincore/madvise** | 2.30 | mincore/madvise/msync/mlock | 附属 mmap；内存优化 |

**修正说明**:

- pipe2 保持在 T3 头部，虽然评分与高级 IO/时间相同，但因"fork 测试依赖"而优先
- 文件权限提升到 T3，因为安全相关

### Tier 4: 扩展功能与兼容性（总分 2.0-2.3）

> 功能增强型，不影响核心正确性

| 顺序 | 测试组 | 总分 | 包含 syscall | 理由 |
|------|--------|------|-------------|------|
| T4-1 | **readv/writev** | 2.00 | readv/writev/pread64/pwritev/preadv/pwritev | IO 变体；分散聚集 |
| T4-2 | **会话/进程组** | 2.00 | setsid/getsid/setpgid/getpgid | 附属 fork；守护进程需要 |
| T4-3 | **socket 选项** | 2.00 | getsockname/getpeername/getsockopt/setsockopt/shutdown | 连接管理；socket 配置 |
| T4-4 | **socketpair** | 2.00 | socketpair | 同主机的 socket 通信 |
| T4-5 | **共享内存** | 2.05 | shmget/shmat/shmctl/shmdt | System V IPC；高性能 IPC |

**修正说明**:

- 共享内存评分 2.05，应归入 T4 而非 T5

### Tier 5: 低优先级（总分 < 2.0）

> 独立性强，出错影响面窄

| 顺序 | 测试组 | 总分 | 包含 syscall | 理由 |
|------|--------|------|-------------|------|
| T5-1 | **系统信息** | 1.85 | uname/sysinfo/getuid/getgid/setuid/getrandom | 简单查询；getrandom 安全敏感但独立 |
| T5-2 | **消息队列** | 1.70 | msgget/msgsnd/msgrcv/msgctl | System V IPC；使用频率低 |
| T5-3 | **现代 fd** | 1.50 | memfd_create/pidfd_open/pidfd_getfd | Linux 5.x 新特性 |
| T5-4 | **特殊 fd** | 1.20 | eventfd2/signalfd4 | 特殊通知机制 |

**修正说明**:

- 系统信息评分从 1.5 提升到 1.85，因为包含 getrandom
- 消息队列评分 1.70，位置正确

---

## 四、测试文件规划（修正版）

### sys_design.md 已覆盖 → 直接按优先级实施

| 优先级 | 章节 | 测试文件 | 状态 |
|--------|------|----------|------|
| T1-1 | §1 基本 IO | test_basic_io.c | ✅ 已完成 |
| T1-2 | §2 openat | test_openat.c | ✅ 已完成 v2 |
| T1-3 | §3 fork/wait | test_fork.c | 待重写 |
| T1-4 | §4 dup/fcntl | test_dup.c | ✅ 已完成 v2 |
| T1-5 | §5 signal | test_signal.c | 待新建 |
| T1-6 | §6 mmap | test_mmap.c | 待重写 |
| T2-1 | §7 brk | test_brk.c | 待新建 |
| T2-2 | §8 futex | test_futex.c | 待新建 |
| T2-3 | §9 stat | test_stat.c | 待新建 |
| T2-4 | §10 socket | test_accept4.c | ✅ 已完成，待增强 |
| T2-5 | §11 epoll | test_epoll.c | 待新建 |
| T2-6 | §12 目录操作 | test_dir.c | 待新建 |
| T2-7 | §13 信号扩展 | test_signal_adv.c | 待新建 |
| T3-1 | §14 pipe2 | test_pipe2.c | ✅ 已完成，待增强 |
| T3-2 | §15 时间 | test_time.c | 待新建 |
| T3-3 | §16 高级 IO | test_adv_io.c | 待新建 |
| T3-4 | §17 定时器 | test_timer.c | 待新建 |
| T3-5 | §18 socket IO | test_socket_io.c | 待新建 |
| T3-6 | §19 select/poll | test_poll.c | 待新建 |
| T3-7 | §20 文件权限 | test_fileperm.c | 待新建 |
| T3-8 | §21 mincore/madvise | test_mmap_adv.c | 待新建 |
| T4-1 | §22 readv/writev | test_iov.c | 待新建 |
| T4-2 | §23 会话/进程组 | test_session.c | 待新建 |
| T4-3 | §24 socket 选项 | test_sockopt.c | 待新建 |
| T4-4 | §25 socketpair | test_socketpair.c | 待新建 |
| T4-5 | §26 共享内存 | test_shm.c | 待新建 |
| T5-1 | §27 系统信息 | test_sysinfo.c | 待新建 |
| T5-2 | §28 消息队列 | test_msg.c | 待新建 |
| T5-3 | §29 现代 fd | test_modern_fd.c | 待新建 |
| T5-4 | §30 特殊 fd | test_special_fd.c | 待新建 |

**修正说明**:

1. **新增章节**: 原 sys_design.md 未覆盖的测试组已全部补充
2. **状态更新**: test_basic_io.c 标记为已完成
3. **文件命名**: 采用简洁命名规范（test_xxx.c）
4. **总数**: 30 个测试文件对应 30 个测试组

---

## 五、排序核心原则总结（修正版）

1. **基础设施优先**：openat/read/write/close/stat 是所有测试的前置条件
2. **高风险优先**：fork/mmap/signal/futex 出错 = 内核崩溃或安全漏洞
3. **依赖链排序**：pipe → fork+pipe 通信 → socket → epoll
4. **附属合并**：会话/进程组合入 fork 测试；socket IO/选项合入 socket 测试
5. **低频后置**：System V IPC、特殊 fd、现代 fd 等独立性强、出错影响小的最后测
6. **评分与 tier 一致**：同一 tier 内按分数降序排列
7. **特殊考虑**：stat 虽分数 2.65 但因"观测手段"属性保留在 T2；getrandom 提升系统信息评分

---

## 六、修订历史

| 版本 | 日期 | 修订内容 |
|------|------|----------|
| v1.0 | 2024-04-12 | 初始版本 |
| v1.1 | 2024-04-14 | 修正评分权重（S: 30%→35%, F: 20%→15%）；修复 tier/分数不一致；补充缺失测试组；更新测试文件规划 |
