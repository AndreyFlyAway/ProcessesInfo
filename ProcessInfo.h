#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <math.h>
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

#define MAXLEN 1000
#define MAXPIDLEN 1000
#define PID_LEN   6

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
int resources_usage(std::vector <proc_info> *config, int n_freq, int slepp_t_ms);
void get_cpu_section(std::vector <process_info_t> &config, int out_arr[]);
void get_mem_section(std::vector <proc_info> *config, int out_virt_arr[], int out_rss_arr[], float out_rss_perc[]);
int MEM_usage(const std::string & pid, int & virt_out, int & rss_out, float & rss_percentage);
std::string get_num_frm_str(const std::string & str);
int load_config(std::vector <process_info_t> &config, const std::string & conf_file_path);
int copy_word(char * str_in, char * str_out);
std::string get_total_mem(void);
int get_system_info(char * str_out);
int & proc_stat(int &cpu_total, int &idle);
std::vector <std::string> & split_string(const std::string & str,
                                    std::vector <std::string> & buf,
                                    const std::string & sp_str=" ");
//int check_process(void);
