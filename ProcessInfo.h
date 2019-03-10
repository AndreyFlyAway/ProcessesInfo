#include <string.h>
#include <dirent.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>


//#include <INIReader.cpp>
#include "cpp/INIReader.h"
//#include "ini.h"
//#include "/home/user/opt/inih/cpp/INIReader.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>
#include <log4cplus/helpers/fileinfo.h>
#include <log4cplus/syslogappender.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/asyncappender.h>

using namespace log4cplus;
using namespace log4cplus::helpers;


#ifndef PROCESSINFO__H_
#define PROCESSINFO__H_

namespace process_info
{

    #define MAXLEN 1000
    #define MAXPIDLEN 1000
    #define PID_LEN   6
    #define PROC_FOLDER   "/proc/"
    #define PROC_NAME                   "/comm"
    #define PROC_STAT_FOLDER   "/proc/stat"
    #define MEMINFO_FOLDER   "/proc/meminfo"
    #define N_FREQ  5
    #define SLEEP_DISCR_MC   200000 // для ожидания во время сбора инофрмации о процессе
    #define SLEEP_PID_ERROR_MC   300000 // время оиждания между попытками найти PID,
    #define LOGGER_NAME "log"
    #define STATUS_FILE_LINES 41
    #define SYS_FILE_NUM 4

    /* list of file in /proc/sys/kernel/, they are used for reading information about distributive */
    const std::string sys_info_list[SYS_FILE_NUM] = {"/proc/sys/kernel/ostype",
                                                     "/proc/sys/kernel/hostname",
                                                     "/proc/sys/kernel/osrelease",
                                                     "/proc/sys/kernel/version", };

    const std::string DEFAULT_CONFIG_FILE_PATH {"config_processes.ini"};

    static long int g_n_freq {}; // количество интервалов
    static long int g_sleep_discr {}; // время между интервалами
    static long int g_average_n = {}; // количество интервалов, которые будет учитываться для вычисления среднего значение

    static int average_counter {}; // для подсчте среднего значения
    static float cpu_load {1.1};
    static long g_total_mem {}; /* для хранения количества памяти на машине */

    /* структура для хранения памят иданных о процессе */
    struct proc_info
    {
    //    std::string name[MAXLEN];
        char name[MAXLEN];
        char pid[PID_LEN];
        float cur_CPU_usage = 0;
        float max_CPU_usage = 0;
        float average_CPU = 0;
        float sum_CPU_values = 0;
        int virt_mem = -1;
        int rss_mem = -1;
        float mem_in_percent;
    };

    struct process_info_t
    {
    //    std::string name[MAXLEN];
        std::string name {};
        std::string pid {};
        float cur_CPU_usage = 0;
        float max_CPU_usage = 0;
        float average_CPU = 0;
        float sum_CPU_values = 0;
        int virt_mem = -1;
        int rss_mem = -1;
        float mem_in_percent;
    };

    std::string get_pid(const std::string & proc_name);
    int process_cpu(const std::string &pid_c);
    int resources_usage(std::vector <process_info_t> &config, const int n_freq, const int sleep_t_ms);
    void get_cpu_section(std::vector <process_info_t> &config, int out_arr[]);
    void get_mem_section(std::vector <process_info_t> &config, int o_virt_arr[], int o_rss_arr[], float o_rss_perc_arr[]);
    int MEM_usage(const std::string & pid, int & virt_out, int & rss_out, float & rss_percentage);
    std::string get_num_frm_str(const std::string & str);
    int load_config(std::vector <process_info_t> &config, const std::string & conf_file_path);
    int copy_word(char * str_in, char * str_out);
    long get_total_mem(void);
    int get_system_info(char * str_out);
    int & proc_stat(int &cpu_total, int &idle);
    std::vector <std::string> & split_string(const std::string & str,
                                        std::vector <std::string> & buf,
                                        const std::string & sp_str=" ");
    //int check_process(void);
}

#endif
