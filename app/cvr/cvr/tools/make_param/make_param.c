#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>         /* for PATH_MAX */
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <getopt.h>
#include <stdbool.h>  // 添加bool类型支持
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>         // 添加这个头文件用于va_start和va_end


#define PARAM_MAGIC 0x4D524150  // "PARA"
#define MAX_FILES 8
#define MAX_FILENAME_LEN 64
#define RESERVED_SIZE (2 * 1024)  // 每个文件额外预留2KB
#define HEADER_SIZE sizeof(ParamHeader)

// 文件信息结构体
typedef struct {
    char filename[MAX_FILENAME_LEN];  // 文件名
    uint32_t size;                    // 实际大小
    uint32_t offset;                  // 文件偏移
    uint32_t crc32;                   // 校验和
} FileInfo;

// 参数镜像头部结构体
typedef struct {
    uint32_t magic;          // 魔数
    uint32_t version;        // 版本号
    uint32_t file_count;     // 文件数量
    uint32_t total_size;     // 总大小
    uint32_t header_crc;     // 头部校验和
    FileInfo files[MAX_FILES];// 文件信息数组
} ParamHeader;

// 添加debug控制
static bool g_debug_enabled = false;

// 添加debug打印函数
static void debug_print(const char *format, ...) {
    if (!g_debug_enabled) return;

    va_list args;
    va_start(args, format);
    printf("[DEBUG] ");
    vprintf(format, args);
    va_end(args);
}

// 计算文件需要的存储空间（实际大小 + 预留空间）
static uint32_t calc_file_space(uint32_t file_size) {
    return file_size + RESERVED_SIZE;
}

// 计算下一个文件的偏移位置
static uint32_t calc_next_offset(uint32_t current_offset, uint32_t file_size) {
    return current_offset + calc_file_space(file_size);
}

// 创建param.img
int create_param_img(const char *ini_dir, const char *output_path) {
    printf("\n=== Creating param.img ===\n");

    ParamHeader header = {
        .magic = PARAM_MAGIC,
        .version = 0x0100,
        .file_count = 0
    };

    // 创建输出文件
    char output_file[PATH_MAX];
    snprintf(output_file, sizeof(output_file), "%s/param.img", output_path);
    int fd = open(output_file, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("Failed to create param.img\n");
        return -1;
    }

    // 写入头部占位
    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
        printf("Failed to write header\n");
        close(fd);
        return -1;
    }

    // 遍历并写入ini文件
    DIR *dir = opendir(ini_dir);
    if (!dir) {
        printf("Failed to open directory\n");
        close(fd);
        return -1;
    }

    struct dirent *entry;
    uint32_t current_offset = sizeof(header);

    printf("\nProcessing files:\n");
    printf("%-4s %-32s %-8s %-8s %-8s\n", "No.", "Filename", "Size", "Space", "Offset");
    printf("----------------------------------------------------------------\n");

    while ((entry = readdir(dir)) != NULL && header.file_count < MAX_FILES) {
        if (strstr(entry->d_name, ".ini") == NULL) continue;

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", ini_dir, entry->d_name);

        // 读取文件内容
        FILE *fp = fopen(filepath, "rb");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        // 计算需要的存储空间
        uint32_t space_needed = calc_file_space(file_size);
        uint8_t *buffer = malloc(space_needed);
        if (!buffer) {
            printf("Failed to allocate buffer for: %s\n", entry->d_name);
            fclose(fp);
            continue;
        }

        // 清空缓冲区
        memset(buffer, 0xFF, space_needed);

        // 读取文件内容
        if (fread(buffer, 1, file_size, fp) != file_size) {
            printf("Failed to read file: %s\n", entry->d_name);
            free(buffer);
            fclose(fp);
            continue;
        }
        fclose(fp);

        // 更新文件信息
        FileInfo *info = &header.files[header.file_count];
        strncpy(info->filename, entry->d_name, MAX_FILENAME_LEN - 1);
        info->size = file_size;
        info->offset = current_offset;
        info->crc32 = 0; // TODO: 添加CRC32计算

        printf("[%2d] %-32s %-8d %-8d 0x%04x\n",
               header.file_count + 1, info->filename, info->size,
               space_needed, info->offset);

        // 写入文件内容（包括预留空间）
        if (write(fd, buffer, space_needed) != space_needed) {
            printf("Failed to write file content\n");
            free(buffer);
            continue;
        }

        free(buffer);
        current_offset = calc_next_offset(current_offset, file_size);
        header.file_count++;
    }

    closedir(dir);

    // 更新总大小
    header.total_size = current_offset;

    // 更新头部
    lseek(fd, 0, SEEK_SET);
    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
        printf("Failed to update header\n");
        close(fd);
        return -1;
    }

    printf("\nParam.img created successfully:\n");
    printf("- Total files: %d\n", header.file_count);
    printf("- Total size: %d bytes\n", header.total_size);

    close(fd);
    return 0;
}

// 确保目录存在
static int ensure_directory(const char *path) {
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
    return system(cmd);
}

// 读取并导出param.img内容
int dump_param_img(const char *img_path) {
    debug_print("\n=== Reading param.img ===\n");
    debug_print("Opening partition: %s\n", img_path);

    const char *output_dir = "/tmp/rkadk";
    int fd = open(img_path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open %s: %s\n", img_path, strerror(errno));
        return -1;
    }

    // 创建输出目录
    if (ensure_directory(output_dir) != 0) {
        printf("Failed to create directory: %s\n", output_dir);
        close(fd);
        return -1;
    }

    // 读取头部
    ParamHeader header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        printf("Failed to read header\n");
        close(fd);
        return -1;
    }

    // 验证魔数
    if (header.magic != PARAM_MAGIC) {
        printf("Invalid magic number: 0x%08X\n", header.magic);
        close(fd);
        return -1;
    }

    debug_print("File count: %u\n", header.file_count);
    debug_print("Total size: %u bytes\n\n", header.total_size);
    debug_print("Exporting files to %s:\n", output_dir);

    // 读取每个文件的内容
    int total_bytes = 0;
    for (uint32_t i = 0; i < header.file_count; i++) {
        FileInfo *info = &header.files[i];
        debug_print("\nProcessing file %u/%u:\n", i + 1, header.file_count);
        debug_print("- Filename: %s\n", info->filename);
        debug_print("- Size: %u bytes\n", info->size);
        debug_print("- Offset: 0x%x\n", info->offset);
        debug_print("- Available space: %u bytes\n",
                   calc_file_space(info->size) - info->size);

        // 只读取实际的文件内容
        uint8_t *content = malloc(info->size);
        if (!content) {
            printf("Failed to allocate buffer\n");
            continue;
        }

        // 读取文件内容（不包括预留空间）
        if (pread(fd, content, info->size, info->offset) == info->size) {
            // 构建输出文件路径
            char output_path[PATH_MAX];
            snprintf(output_path, sizeof(output_path), "%s/%s",
                    output_dir, info->filename);

            // 写入文件
            FILE *fp = fopen(output_path, "wb");
            if (fp) {
                if (fwrite(content, 1, info->size, fp) == info->size) {
                    debug_print("- Exported to: %s\n", output_path);
                    debug_print("Content:\n%.*s\n\n", info->size, (char *)content);
                    total_bytes += info->size;
                } else {
                    printf("Failed to write file: %s\n", output_path);
                }
                fclose(fp);

                // 设置文件权限
                chmod(output_path, 0644);
            } else {
                printf("Failed to create file: %s\n", output_path);
            }
        } else {
            printf("Failed to read file content\n");
        }

        free(content);
    }

    debug_print("\nExport completed: %u files, %d bytes total\n",
                header.file_count, total_bytes);

    close(fd);
    return 0;
}

// 回写/tmp/rkadk下的所有ini文件到param.img
int writeback_param_img(const char *img_path) {
    debug_print("Entering writeback_param_img, path: %s\n", img_path);
    printf("\n=== Writing back param.img ===\n");

    const char *input_dir = "/tmp/rkadk";
    DIR *dir = opendir(input_dir);
    if (!dir) {
        printf("Failed to open directory: %s\n", input_dir);
        return -1;
    }

    printf("Opening partition: %s\n", img_path);
    int fd = open(img_path, O_RDWR);
    if (fd < 0) {
        printf("Failed to open %s: %s\n", img_path, strerror(errno));
        closedir(dir);
        return -1;
    }

    // 读取并验证头部
    ParamHeader header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        printf("Failed to read header\n");
        close(fd);
        closedir(dir);
        return -1;
    }

    if (header.magic != PARAM_MAGIC) {
        printf("Invalid magic number: 0x%08X\n", header.magic);
        close(fd);
        closedir(dir);
        return -1;
    }

    printf("\nWriting back files from %s to %s:\n", input_dir, img_path);
    printf("%-4s %-32s %-8s %-8s %-8s\n", "No.", "Filename", "Old Size", "New Size", "Space");
    printf("----------------------------------------------------------------\n");

    struct dirent *entry;
    int updated_count = 0;
    int total_bytes = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG || strstr(entry->d_name, ".ini") == NULL) {
            continue;
        }

        // 查找对应的文件记录
        FileInfo *target_file = NULL;
        for (uint32_t i = 0; i < header.file_count; i++) {
            if (strcmp(header.files[i].filename, entry->d_name) == 0) {
                target_file = &header.files[i];
                break;
            }
        }

        if (!target_file) {
            debug_print("File %s not found in param.img\n", entry->d_name);
            continue;
        }

        debug_print("File info: size=%u, offset=0x%x\n",
                   target_file->size, target_file->offset);

        // 读取新文件
        char input_path[PATH_MAX];
        snprintf(input_path, sizeof(input_path), "%s/%s", input_dir, entry->d_name);

        struct stat st;
        if (stat(input_path, &st) != 0) {
            printf("Failed to get file size: %s\n", input_path);
            continue;
        }

        // 检查文件大小（使用总空间大小进行比较）
        uint32_t total_space = calc_file_space(target_file->size);
        if (st.st_size > total_space) {
            printf("Error: %s too large (%ld > %u bytes, total space including reserve)\n",
                   entry->d_name, st.st_size, total_space);
            continue;
        }

        // 分配缓冲区（使用总空间大小）
        uint8_t *buffer = malloc(total_space);
        if (!buffer) {
            printf("Failed to allocate buffer for %s\n", entry->d_name);
            continue;
        }

        // 清空缓冲区（包括预留空间）
        memset(buffer, 0xFF, total_space);

        // 读取新文件内容
        FILE *fp = fopen(input_path, "rb");
        if (!fp) {
            printf("Failed to open %s\n", input_path);
            free(buffer);
            continue;
        }

        if (fread(buffer, 1, st.st_size, fp) != st.st_size) {
            printf("Failed to read %s\n", input_path);
            fclose(fp);
            free(buffer);
            continue;
        }
        fclose(fp);

        printf("[%2d] %-32s %-8u %-8ld %-8u\n",
               updated_count + 1, entry->d_name,
               target_file->size, st.st_size, total_space);

        // 更新文件大小
        uint32_t old_size = target_file->size;
        target_file->size = st.st_size;

        // 写入新内容（包括预留空间）
        if (pwrite(fd, buffer, total_space, target_file->offset) != total_space) {
            printf("Failed to write content for %s\n", entry->d_name);
            target_file->size = old_size;  // 恢复原大小
            free(buffer);
            continue;
        }

        total_bytes += st.st_size;
        updated_count++;

        free(buffer);
    }

    // 更新头部
    lseek(fd, 0, SEEK_SET);
    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
        printf("Failed to update header\n");
        close(fd);
        closedir(dir);
        return -1;
    }

    printf("\nWriteback completed:\n");
    printf("- Files processed: %d\n", updated_count);
    printf("- Total bytes: %d\n", total_bytes);

    close(fd);
    closedir(dir);
    return 0;
}

// 添加测试函数
static int test_param_operations(const char *partition_path) {
    debug_print("Starting test operations on partition: %s\n", partition_path);
    int test_count = 0;
    printf("\n=== Starting param read/write test ===\n");

    while (1) {
        test_count++;
        printf("\nTest iteration %d:\n", test_count);

        // 1. 读取参数分区
        printf("\n[Step 1] Reading parameters...\n");
        if (dump_param_img(partition_path) != 0) {
            printf("Failed to read parameters\n");
            return -1;
        }

        // 2. 修改配置文件
        printf("\n[Step 2] Modifying parameters...\n");
        char cmd[256];
        // 修改speaker_volume的值在50-100之间随机变化
        int new_volume = 50 + (rand() % 51);
        debug_print("Setting new speaker_volume: %d\n", new_volume);
        snprintf(cmd, sizeof(cmd),
                "sed -i 's/speaker_volume.*= .*/speaker_volume                 = %d/' "
                "/tmp/rkadk/rkadk_setting.ini",
                new_volume);
        if (system(cmd) != 0) {
            printf("Failed to modify parameters\n");
            return -1;
        }

        // 3. 回写参数
        printf("\n[Step 3] Writing back parameters...\n");
        if (writeback_param_img(partition_path) != 0) {
            printf("Failed to write back parameters\n");
            return -1;
        }

        // 4. 再次读取验证
        printf("\n[Step 4] Verifying changes...\n");
        if (dump_param_img(partition_path) != 0) {
            printf("Failed to verify parameters\n");
            return -1;
        }

        printf("\nTest iteration %d completed\n", test_count);
        printf("Press Ctrl+C to stop the test\n");
        sleep(1);  // 等待1秒再开始下一轮测试
    }

    return 0;
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  --help                   显示此帮助信息\n");
    printf("  --writeback              回写/tmp/rkadk下的ini文件到分区\n");
    printf("  --test                   启动读写测试循环\n");
    printf("  --debug                  启用调试信息输出\n");  // 添加debug选项说明
    printf("  --partition <path>       指定参数分区路径 (默认: /dev/mmcblk0p5)\n");
    printf("  --ini_dir <path>         指定ini文件目录 (仅PC端)\n");
    printf("  --output <path>          指定输出目录 (仅PC端)\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    int c;
    int option_index = 0;
    bool do_writeback = false;
    bool do_test = false;  // 添加测试标志
    const char *partition_path = "/dev/mmcblk0p5";
    const char *ini_dir = NULL;
    const char *output_dir = NULL;

    static struct option long_options[] = {
        {"help",      no_argument,       0, 'h'},
        {"writeback", no_argument,       0, 'w'},
        {"test",      no_argument,       0, 't'},  // 添加测试选项
        {"debug",     no_argument,       0, 'd'},  // 添加debug选项
        {"partition", required_argument, 0, 'p'},
        {"ini_dir",   required_argument, 0, 'i'},
        {"output",    required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };

#ifdef RUN_ON_BOARD
    while ((c = getopt_long(argc, argv, "hwtdp:",
           long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'w':
                do_writeback = true;
                break;
            case 't':  // 添加测试选项处理
                do_test = true;
                break;
            case 'd':  // 处理debug选项
                g_debug_enabled = true;
                debug_print("Debug mode enabled\n");
                break;
            case 'p':
                partition_path = optarg;
                break;
            case '?':
                print_usage(argv[0]);
                return -1;
            default:
                printf("Unknown option: %c\n", c);
                print_usage(argv[0]);
                return -1;
        }
    }

    if (do_test) {
        srand(time(NULL));  // 初始化随机数生成器
        return test_param_operations(partition_path);
    } else if (do_writeback) {
        printf("Writing back to partition: %s\n", partition_path);
        return writeback_param_img(partition_path);
    } else {
        debug_print("Reading from partition: %s\n", partition_path);
        return dump_param_img(partition_path);
    }
#else
    while ((c = getopt_long(argc, argv, "hi:o:",
           long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'i':
                ini_dir = optarg;
                break;
            case 'o':
                output_dir = optarg;
                break;
            case '?':
                print_usage(argv[0]);
                return -1;
            default:
                printf("Unknown option: %c\n", c);
                print_usage(argv[0]);
                return -1;
        }
    }

    if (!ini_dir || !output_dir) {
        printf("Error: --ini_dir and --output are required\n");
        print_usage(argv[0]);
        return -1;
    }

    // 检查ini目录是否存在
    DIR *dir = opendir(ini_dir);
    if (!dir) {
        printf("Error: Cannot open ini directory '%s': %s\n",
               ini_dir, strerror(errno));
        return -1;
    }
    closedir(dir);

    // 检查输出目录是否存在
    dir = opendir(output_dir);
    if (!dir) {
        printf("Error: Cannot open output directory '%s': %s\n",
               output_dir, strerror(errno));
        return -1;
    }
    closedir(dir);

    printf("Creating param.img:\n");
    printf("  INI directory: %s\n", ini_dir);
    printf("  Output directory: %s\n", output_dir);

    return create_param_img(ini_dir, output_dir);
#endif
}