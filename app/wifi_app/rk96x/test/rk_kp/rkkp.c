#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <paths.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include "utility.h"

typedef struct
{
    char key;
    char cmdline[32];
} rkkp_cb_t;


// function list table
rkkp_cb_t rkkp_cb_list[ ] =
{
    // rk kill process
    {'b', "bt_start.sh"},
    {'w', "wifi_start.sh"},
    {'t', "sys_test.sh"},
    {'s', "rkwifi_server"},
    {'u', "btmcu_daemon"},
    {'i', "iperf"},
    // rk set process
    {'n', "set_wifi_no_sleep 0"},
    {'y', "set_wifi_no_sleep 1"},
    {'0', "set_linux_kernel_log 0"},
    {'7', "set_linux_kernel_log 7"},
    {'1', "set_wifi_sdio_width 1"},
    {'4', "set_wifi_sdio_width 4"},
    {'5', "set_bt_start_mode 1"},
    {'6', "set_bt_start_mode 0"},
    {'3', "set_wifi_start_mode 1"},
    {'2', "set_wifi_start_mode 0"},
    {'f', "set_wifi_rf_log_output 0"},
    {'g', "set_wifi_rf_log_output 1"},
    {'p', "set_bt_rf_log_output 0"},
    {'q', "set_bt_rf_log_output 1"},
    // rkkp help
    {'h', "help"}
};

static void usage(void)
{
    printf(
        "Usage: rkkp OPTION:\n"
        "  5, set bt_auto_start=1\n"
        "  6, set bt_auto_start=0\n"
        "  3, set wifi_auto_start=1\n"
        "  2, set wifi_auto_start=0\n"
        "  0, set linux_kernel_log=0\n"
        "  7, set linux_kernel_log=7\n"
        "  b, Kill bt_mcu auto-start\n"
        "  w, Kill wifi auto-start\n"
        "  t, Kill system test\n"
        "  s, Kill rkwifi_server\n"
        "  u, Kill btmcu_daemon\n"
        "  i, Kill iperf\n"
        "  n, Set wifi fw_no_sleep=0\n"
        "  y, Set wifi fw_no_sleep=1\n"
        "  1, Set wifi sdio_width=1\n"
        "  4, Set wifi sdio_width=4\n"
        "  f, Set wifi_rf_log=0\n"
        "  g, Set wifi_rf_log=1\n"
        "  p, Set bt_rf_log=0\n"
        "  q, Set bt_rf_log=1\n"
        "  h, Git help information\n"
    );
}

static bool get_wifi_user_conf(char *buf, size_t len)
{
    char *rk96x_conf_path = getenv("RK96X_CONF_PATH");
    if (rk96x_conf_path == NULL)
    {
        printf("RK96X_CONF_PATH not found.\n");
        return false;
    }

    snprintf(buf, len-1, "%s/wifi_user.conf", rk96x_conf_path);
    printf("WIFI_USER_CONFIG_FILE: '%s'\n", buf);

    return true;
}

static bool get_bt_user_conf(char *buf, size_t len)
{
    char *rk96x_conf_path = getenv("RK96X_CONF_PATH");
    if (rk96x_conf_path == NULL)
    {
        printf("RK96X_CONF_PATH not found.\n");
        return false;
    }

    snprintf(buf, len-1, "%s/bt_user.conf", rk96x_conf_path);
    printf("BT_USER_CONFIG_FILE: '%s'\n", buf);

    return true;
}

static void set_wifi_no_sleep(int nosleep)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confs";
    int found = 0;

    char wifi_user_conf_file[128] = {0};
    if (!get_wifi_user_conf(wifi_user_conf_file, sizeof(wifi_user_conf_file))) {
        printf("get_wifi_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(wifi_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "fw_no_sleep=", strlen("fw_no_sleep=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "fw_no_sleep=%d\n", nosleep);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "fw_no_sleep=%d\n", nosleep);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name  
    remove(wifi_user_conf_file);
    rename(temp_filename, wifi_user_conf_file);

    printf("******************************\n");
    printf("** Set wifi fw_no_sleep=%d !!!\n", nosleep);
    printf("******************************\n");

    return;
}

static void set_wifi_sdio_width(int width)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confd";
    int found = 0;

    char wifi_user_conf_file[128] = {0};
    if (!get_wifi_user_conf(wifi_user_conf_file, sizeof(wifi_user_conf_file))) {
        printf("get_wifi_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(wifi_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "sdio_width=", strlen("sdio_width=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "sdio_width=%d\n", width);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "sdio_width=%d\n", width);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(wifi_user_conf_file);
    rename(temp_filename, wifi_user_conf_file);

    printf("******************************\n");
    printf("** Set wifi sdio_width=%d !!!\n", width);
    printf("******************************\n");

    return;
}

static void set_bt_start_mode(int mode)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confb";
    int found = 0;

    char bt_user_conf_file[128] = {0};
    if (!get_bt_user_conf(bt_user_conf_file, sizeof(bt_user_conf_file))) {
        printf("get_bt_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(bt_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "bt_auto_start=", strlen("bt_auto_start=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "bt_auto_start=%d\n", mode);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "bt_auto_start=%d\n", mode);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(bt_user_conf_file);
    rename(temp_filename, bt_user_conf_file);

    printf("******************************\n");
    printf("** Set bt_auto_start=%d !!!\n", mode);
    printf("******************************\n");

    return;
}

static void set_wifi_start_mode(int mode)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confw";
    int found = 0;

    char wifi_user_conf_file[128] = {0};
    if (!get_wifi_user_conf(wifi_user_conf_file, sizeof(wifi_user_conf_file))) {
        printf("get_wifi_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(wifi_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "wifi_auto_start=", strlen("wifi_auto_start=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "wifi_auto_start=%d\n", mode);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "wifi_auto_start=%d\n", mode);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(wifi_user_conf_file);
    rename(temp_filename, wifi_user_conf_file);

    printf("******************************\n");
    printf("** Set wifi_auto_start=%d !!!\n", mode);
    printf("******************************\n");

    return;
}

static void set_wifi_rf_log_output(int flag)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confr";
    int found = 0;

    char wifi_user_conf_file[128] = {0};
    if (!get_wifi_user_conf(wifi_user_conf_file, sizeof(wifi_user_conf_file))) {
        printf("get_wifi_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(wifi_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "wifi_rf_log=", strlen("wifi_rf_log=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "wifi_rf_log=%d\n", flag);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "wifi_rf_log=%d\n", flag);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(wifi_user_conf_file);
    rename(temp_filename, wifi_user_conf_file);

    printf("******************************\n");
    printf("** Set wifi_rf_log=%d !!!\n", flag);
    printf("******************************\n");

    return;
}

static void set_bt_rf_log_output(int flag)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_conff";
    int found = 0;

    char bt_user_conf_file[128] = {0};
    if (!get_bt_user_conf(bt_user_conf_file, sizeof(bt_user_conf_file))) {
        printf("get_bt_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(bt_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // try open initial config file
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "bt_rf_log=", strlen("bt_rf_log=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "bt_rf_log=%d\n", flag);
            found = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // add new line if no found item
    if (!found)
    {
        fprintf(fp_temp, "bt_rf_log=%d\n", flag);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(bt_user_conf_file);
    rename(temp_filename, bt_user_conf_file);

    printf("******************************\n");
    printf("** Set bt_rf_log=%d !!!\n", flag);
    printf("******************************\n");

    return;
}

static void set_kernel_log_output(int flag)
{
    FILE *fp, *fp_temp;
    char line[256] = {0};
    char temp_filename[] = "/data/temp_confk";
    int found_log = 0;
    int found_level = 0;
	int default_level = 0x6;

    char wifi_user_conf_file[128] = {0};
    if (!get_wifi_user_conf(wifi_user_conf_file, sizeof(wifi_user_conf_file))) {
        printf("get_wifi_user_conf failed.\n");
        return;
    }

    // try open initial config file
    fp = fopen(wifi_user_conf_file, "r+");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    // create temp file to write
    fp_temp = fopen(temp_filename, "w+");
    if (fp_temp == NULL)
    {
        perror("Error creating temporary file");
        fclose(fp);
        return;
    }

    // if flag is 7, set level is 0xe
    if(flag == 7)
    {
        default_level = 0xe;
    }    

    // read initial config to modify
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // line to need to modify
        if (strncmp(line, "linux_kernel_log=", strlen("linux_kernel_log=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "linux_kernel_log=%d\n", flag);
            found_log = 1;
        }
        else if (strncmp(line, "debug_level=", strlen("debug_level=")) == 0)
        {
            // get the line and update value
            fprintf(fp_temp, "debug_level=0x%x\n", default_level);
            found_level = 1;
        }
        else
        {
            // other lines keep un-changed
            fputs(line, fp_temp);
        }
    }

    // no found item, add new one
    if (!found_log)
    {
        fprintf(fp_temp, "linux_kernel_log=%d\n", flag);
    }

    if (!found_level)
    {
        fprintf(fp_temp, "debug_level=0x%x\n", default_level);
    }

    // close file
    fclose(fp);
    fclose(fp_temp);

    // mv temp's name to initial file's name
    remove(wifi_user_conf_file);
    rename(temp_filename, wifi_user_conf_file);

    printf("******************************\n");
    printf("** Set linux_kernel_log=%d !!!\n", flag);
    printf("** Set debug_level=0x%x !!!\n", default_level);
    printf("******************************\n");

    return;
}

int main(int argc, char *argv[])
{

    unsigned int index = 0;
    int flag = 0;

    if ((argc != 2) || (strlen(argv[1]) > 1) || (!strncmp("h", argv[1], sizeof("h"))))
    {
        usage();
        return 0;
    }

    /////////////////////////////////////////////////////
    //     A: Modified configuration
    /////////////////////////////////////////////////////
    // fw_no_sleep=0
    if (!strncmp("n", argv[1], sizeof("n")))
    {
        set_wifi_no_sleep(0);
        return 0;
    }

    // fw_no_sleep=1
    if (!strncmp("y", argv[1], sizeof("y")))
    {
        set_wifi_no_sleep(1);
        return 0;
    }

    // sdio_width=1
    if (!strncmp("1", argv[1], sizeof("1")))
    {
        set_wifi_sdio_width(1);
        return 0;
    }

    // sdio_width=4
    if (!strncmp("4", argv[1], sizeof("4")))
    {
        set_wifi_sdio_width(4);
        return 0;
    }

    // bt_auto_start=1
    if (!strncmp("5", argv[1], sizeof("5")))
    {
        set_bt_start_mode(1);
        return 0;
    }

    // bt_auto_start=0
    if (!strncmp("6", argv[1], sizeof("6")))
    {
        set_bt_start_mode(0);
        return 0;
    }

    // wifi_auto_start=1
    if (!strncmp("3", argv[1], sizeof("3")))
    {
        set_wifi_start_mode(1);
        return 0;
    }

    // wifi_auto_start=0
    if (!strncmp("2", argv[1], sizeof("2")))
    {
        set_wifi_start_mode(0);
        return 0;
    }

    // linux_kernel_log=0
    if (!strncmp("0", argv[1], sizeof("0")))
    {
        set_kernel_log_output(0);
        return 0;
    }

    // linux_kernel_log=7
    if (!strncmp("7", argv[1], sizeof("7")))
    {
        set_kernel_log_output(7);
        return 0;
    }

    // wifi_rf_log=1
    if (!strncmp("f", argv[1], sizeof("f")))
    {
        set_wifi_rf_log_output(0);
        return 0;
    }

    // wifi_rf_log=0
    if (!strncmp("g", argv[1], sizeof("g")))
    {
        set_wifi_rf_log_output(1);
        return 0;
    }

    // bt_rf_log=0
    if (!strncmp("p", argv[1], sizeof("p")))
    {
        set_bt_rf_log_output(0);
        return 0;
    }

    // bt_rf_log=1
    if (!strncmp("q", argv[1], sizeof("q")))
    {
        set_bt_rf_log_output(1);
        return 0;
    }

    /////////////////////////////////////////////////////
    //     B: Terminate procedures (kill process)
    /////////////////////////////////////////////////////
    for (index = 0; index < (sizeof(rkkp_cb_list) / sizeof(rkkp_cb_t)); index++)
    {
        if (rkkp_cb_list[index].key == argv[1][0])
        {
            util_kill_process_by_name(rkkp_cb_list[index].cmdline);
            flag = 1;
        }
    }

    if (!flag)
    {
        usage();
    }

    return 0;
}

