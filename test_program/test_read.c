/*
 * test_read.c — read 系统调用完整测试
 *
 * 覆盖:
 *   正向: read 从文件、read /dev/null
 *   负向: EBADF (无效 fd)、零字节读取
 */

#include "test_framework.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

int main(void)
{
    TEST_START("read: 基本读取 + 错误路径");

    /* ================================================================
     * 1. read 从无效 fd → EBADF
     * ================================================================ */

    char dummy[16];
    errno = 0;
    CHECK_ERR(read(-1, dummy, sizeof(dummy)), EBADF, "read(-1) → EBADF");

    errno = 0;
    CHECK_ERR(read(9999, dummy, 1), EBADF, "read(9999) → EBADF");

    /* ================================================================
     * 2. read 零字节
     * ================================================================ */

    memset(dummy, 0xAA, sizeof(dummy));
    CHECK_RET(read(STDIN_FILENO, dummy, 0), 0,
              "read(stdin, buf, 0) → 0");

    /* ================================================================
     * 3. read 从 /dev/null → 0 (EOF)
     * ================================================================ */

    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0) {
        char null_buf[64];
        memset(null_buf, 0xAA, sizeof(null_buf));
        long ret = read(fd, null_buf, sizeof(null_buf));
        CHECK_RET(ret, 0, "read(/dev/null) → 0 (EOF)");
        close(fd);
    } else {
        printf("  SKIP | %s:%d | /dev/null 不可用\n", __FILE__, __LINE__);
    }

    /* ================================================================
     * 4. read 从已写入的文件
     * ================================================================ */

    const char *tmpfile = "/tmp/starry_read_test";
    unlink(tmpfile);

    /* 先写入 */
    fd = open(tmpfile, O_CREAT | O_RDWR | O_TRUNC, 0644);
    CHECK(fd >= 0, "open 创建测试文件");
    if (fd < 0) {
        printf("  FATAL: 无法创建测试文件，终止\n");
        TEST_DONE();
    }

    const char *msg = "Hello read!";
    int msg_len = strlen(msg);
    CHECK_RET(write(fd, msg, msg_len), msg_len, "write 写入测试数据");

    /* 回到文件头读取 */
    CHECK_RET(lseek(fd, 0, SEEK_SET), 0, "lseek 回到文件头");

    char read_buf[64] = {0};
    CHECK_RET(read(fd, read_buf, sizeof(read_buf) - 1), msg_len,
              "read 读回数据");
    CHECK(strcmp(read_buf, msg) == 0, "read 内容匹配");

    close(fd);
    unlink(tmpfile);

    /* ================================================================
     * 5. O_RDONLY fd 上 write → EBADF
     * ================================================================ */

    fd = open(tmpfile, O_CREAT | O_RDONLY, 0644);
    if (fd >= 0) {
        errno = 0;
        CHECK_ERR(write(fd, "x", 1), EBADF, "O_RDONLY fd: write → EBADF");
        close(fd);
    }
    unlink(tmpfile);

    TEST_DONE();
}
