/*
 * test_open.c — open (openat) 基本功能测试
 *
 * 注: 完整语义测试在 test_openat.c 中
 * 本文件聚焦基本正向 + 负向路径，快速验证 openat 核心行为
 *
 * 覆盖:
 *   正向: 创建文件、读写打开、O_CREAT
 *   负向: ENOENT (不存在)、EBADF (无效 dirfd)、EFAULT (NULL 路径)
 */

#include "test_framework.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    TEST_START("open: 基本创建 + 错误路径");

    /* ================================================================
     * 1. openat 打开不存在的文件（无 O_CREAT） → ENOENT
     * ================================================================ */

    errno = 0;
    CHECK_ERR(openat(AT_FDCWD, "/tmp/starry_not_exist_xyz", O_RDONLY),
              ENOENT, "openat 无 O_CREAT 打开不存在文件 → ENOENT");

    /* ================================================================
     * 2. openat O_CREAT 创建文件
     * ================================================================ */

    const char *tmpfile = "/tmp/starry_open_test";
    unlink(tmpfile);

    int fd = openat(AT_FDCWD, tmpfile, O_CREAT | O_WRONLY, 0644);
    CHECK(fd >= 0, "openat O_CREAT+O_WRONLY 创建文件成功");
    if (fd >= 0) {
        close(fd);
    }

    /* 观测: stat 确认文件存在 */
    struct stat st;
    CHECK_RET(stat(tmpfile, &st), 0, "stat 验证文件存在");
    CHECK(st.st_size == 0, "stat 验证文件大小为 0（新创建）");

    /* ================================================================
     * 3. openat O_CREAT | O_RDWR 创建并读写
     * ================================================================ */

    fd = openat(AT_FDCWD, tmpfile, O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK(fd >= 0, "openat O_CREAT+O_RDWR 打开成功");
    if (fd >= 0) {
        const char *msg = "test data";
        int len = strlen(msg);
        CHECK_RET(write(fd, msg, len), len, "write 写入数据");

        /* 回读 */
        CHECK_RET(lseek(fd, 0, SEEK_SET), 0, "lseek 回到文件头");

        char buf[32] = {0};
        CHECK_RET(read(fd, buf, sizeof(buf) - 1), len, "read 读回数据");
        CHECK(strcmp(buf, msg) == 0, "内容匹配");

        close(fd);
    }

    /* ================================================================
     * 4. openat 无效 dirfd + 相对路径 → EBADF
     * ================================================================ */

    errno = 0;
    CHECK_ERR(openat(-1, "relative_file", O_RDONLY),
              EBADF, "openat 无效 dirfd(-1) + 相对路径 → EBADF");

    /* ================================================================
     * 5. openat O_CREAT+O_EXCL 已存在文件 → EEXIST
     * ================================================================ */

    errno = 0;
    CHECK_ERR(openat(AT_FDCWD, tmpfile, O_CREAT | O_EXCL | O_RDWR, 0644),
              EEXIST, "openat O_CREAT+O_EXCL 已存在文件 → EEXIST");

    /* ================================================================
     * 6. openat NULL 路径 → EFAULT
     * ================================================================ */

    errno = 0;
    CHECK_ERR(openat(AT_FDCWD, (const char *)NULL, O_RDONLY),
              EFAULT, "openat NULL 路径 → EFAULT");

    /* ================================================================
     * 7. openat O_DIRECTORY 打开普通文件 → ENOTDIR
     * ================================================================ */

    fd = openat(AT_FDCWD, tmpfile, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        CHECK(0, "O_DIRECTORY 打开普通文件应失败");
        close(fd);
    } else {
        CHECK(errno == ENOTDIR, "O_DIRECTORY 打开普通文件 → ENOTDIR");
    }

    /* 清理 */
    unlink(tmpfile);

    TEST_DONE();
}
