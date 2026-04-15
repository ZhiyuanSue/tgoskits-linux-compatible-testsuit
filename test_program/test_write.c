/*
 * test_write.c — write 系统调用完整测试
 *
 * 覆盖:
 *   正向: write stdout、write 已打开的文件
 *   负向: EBADF (无效 fd)、零字节写入
 */

#include "test_framework.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main(void)
{
    TEST_START("write: 基本写入 + 错误路径");

    /* ================================================================
     * 1. write 到 stdout
     * ================================================================ */

    const char *msg = "hello write\n";
    int msg_len = strlen(msg);
    CHECK_RET(write(STDOUT_FILENO, msg, msg_len), msg_len,
              "write(STDOUT, msg) 返回写入字节数");

    /* ================================================================
     * 2. write 到无效 fd → EBADF
     * ================================================================ */

    errno = 0;
    CHECK_ERR(write(-1, "x", 1), EBADF, "write(-1) → EBADF");

    errno = 0;
    CHECK_ERR(write(9999, "x", 1), EBADF, "write(9999) → EBADF");

    /* ================================================================
     * 3. write 零字节
     * ================================================================ */

    CHECK_RET(write(STDOUT_FILENO, NULL, 0), 0,
              "write(fd, NULL, 0) → 0");

    /* ================================================================
     * 4. write 到已打开的文件
     * ================================================================ */

    const char *tmpfile = "/tmp/starry_write_test";
    unlink(tmpfile);

    int fd = open(tmpfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    CHECK(fd >= 0, "open 创建文件成功");
    if (fd >= 0) {
        const char *file_msg = "write to file";
        int len = strlen(file_msg);
        CHECK_RET(write(fd, file_msg, len), len,
                  "write 到文件 fd");
        CHECK_RET(close(fd), 0, "close 成功");
    }

    /* 观测: stat 验证文件大小 */
    struct stat st;
    if (stat(tmpfile, &st) == 0) {
        CHECK(st.st_size == strlen("write to file"),
              "stat 验证文件大小正确");
    }

    unlink(tmpfile);

    TEST_DONE();
}
