/**
 * @file process_info.cpp
 * @brief вывод данных по процессу - потребляемая память, процессорное время (файл для рефакторинга)
 * @author Yanikeev-AS
 * @version 0.1
 * @date 2018-06-27
 */

#include "process_info.h"
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

#define LOG4CPLUS_CONFIG_NAME "log4cplus_conf.cfg"
const std::string DEFAULT_CONFIG_FILE_PATH {"config_processes.ini"};

std::vector <proc_info> process_arr;
std::vector <process_info_t> g_processes_data;

Logger logger;

/* для хранения настроек */
long int g_n_freq {}; // количество интервалов
long int g_sleep_discr {}; // время между интервалами
long int g_average_n = {}; // количество интервалов, которые будет учитываться для вычисления среднего значение
int average_counter {}; // для подсчте среднего значения
float cpu_load {1.1};
long g_total_mem {}; /* для хранения количества памяти на машине */

/* list of file in /proc/sys/kernel/, they are used for reading information about distributive */
const std::string sys_info_list[SYS_FILE_NUM] = {"/proc/sys/kernel/ostype",
                                                 "/proc/sys/kernel/hostname",
                                                 "/proc/sys/kernel/osrelease",
                                                 "/proc/sys/kernel/version", };

int main(int argc, char * argv[])
{
    std::string str {"kek 12345"};
    std::string buf {};
    buf = get_pid("yakuake");
    std::cout << buf << std::endl;
    load_config(g_processes_data, DEFAULT_CONFIG_FILE_PATH);
    for (u_int i = 0; i < g_processes_data.size() ; i++)
    {
        g_processes_data[i].pid = get_pid(g_processes_data[i].name);
    }

//    for (u_int i = 0; i < g_processes_data.size() ; i++)
//        std::cout << g_processes_data[i].name << ": " << g_processes_data[i].pid << std::endl;

    int res = process_cpu(g_processes_data[0].pid);
    std::cout << res << std::endl;
}

/* получение общего количества памяти на машине */
/**
 *  gets total memory that machine has
 * @return total memrory as std::string object, or text "None" if reading /proc/meminfo is faild
 */
std::string get_total_mem(void)
{
    std::ifstream meminfo_file;
    std::string  meminfo_value {};
    std::string res {"None"};

    meminfo_file.open(MEMINFO_FOLDER);
    if (meminfo_file.is_open())
    {
        std::getline(meminfo_file, meminfo_value);
        res = get_num_frm_str(meminfo_value);

    }
    else
    {
        LOG4CPLUS_INFO_FMT(logger, "Cant read %s",  MEMINFO_FOLDER);
    };

    meminfo_file.close();
    return res;
}

/* сбор информации о версии ядра и сборки, все данные берутся из /proc/sys/kernel/{ostype, hostname,
osrelease, version, domainname} */
/**
 * information about system
 *
 * getting iformation about system - the distibutive build date, version, all information is gotten from /proc/sys/kernel
 * filse ostype, hostname, osrelease, version, domainname
 *
 * @param str_out output buufer where an inormation will be copied
 * @return 0 - if all files is managed to open, number of falls in otherwise .
 */
int get_system_info(char * str_out)
{
    std::ifstream info_file;
    std::string file_contain {};
    int copy_pos {1};
    int res {};

    for(auto file_name : sys_info_list)
    {
        info_file.open(file_name);
        if (info_file.is_open())
        {
            info_file >> file_contain;
            file_contain.append(" ");
            file_contain.copy(str_out + copy_pos, file_contain.size());
            copy_pos += file_contain.size();
        }
        else
            res += 1;
        info_file.close();

    }
    return res;
}


/* копирование первого вхождения слова с цифрами в строке */
/**
 *
 *  copies sequences of numbers in a string
 *
 * @param str copy from
 * @param buf copy to
 * @return number of copied characters
 */
std::string get_num_frm_str(const std::string & str)
{
    std::size_t const n = str.find_first_of("0123456789");
    if (n != std::string::npos)
    {
        std::size_t const m = str.find_first_not_of("0123456789", n);
        return str.substr(n, m != std::string::npos ? m-n : m);
    }
    return std::string();
}

/* загрузка конфигурации */
/**
 * loading configuration
 *
 * loading configuration from ini-file and putting it into vector of type process_info_t
 *
 * @param config vector, where the names of processes is saved
 * @param conf_file_path path to a config file
 * @return 1 in succusses, 0 otherwise
 */
int load_config(std::vector <process_info_t> &config, const std::string & conf_file_path)
{
    INIReader reader(conf_file_path);
    struct process_info_t temp_val {};
    int pos1 {};
    int pos2 {};

    if (reader.ParseError() < 0) {
        return 0;
    }

    g_n_freq = reader.GetInteger("settings", "period_number", -1);
    g_sleep_discr = reader.GetInteger("settings", "period_time_ms", -1);
    g_average_n = reader.GetInteger("settings", "average_period", -1);
    std::string processes = reader.Get("processes", "processes", "UNKNOWN");
    /* удаляю все пробелы из строки, т.к. в дальнейшем разделяю слова по запятым */
    processes.erase(std::remove(processes.begin(), processes.end(), ' '),
                    processes.end());
    while (pos2 != -1)
    {
        pos2 = processes.find(",", pos1);
//        processes.copy(temp_val.name, pos2-pos1, pos1);
        temp_val.name = processes.substr(pos1, pos2-pos1);
//        std::cout << temp_val.name << std::endl;
        pos1 = pos2 + 1;
        config.push_back(temp_val);
        temp_val.name.clear();
    }
    /* хоть и при неверном ini-файое значения у цифирных переменных будут -1, все равно сделал услвоие проверки
    х > 0, дабы защититься от неверных настроек  */
    if ((config.size() == 0) || !(g_n_freq > 0) || !(g_sleep_discr > 0) || !(g_average_n > 0) )
        return 0;

    return 1;
}

/* получение ID процесса по его имени, значение PID сохранятеся в буфер pid, а функция возрващет 1 или -1
если удалось найти процесс */
/**
 * gets process ID by his name
 * @param proc_name process name
 * @return processe ID
 */
std::string get_pid(const std::string & proc_name)
{
    std::string pid {"None"};
    bool pres_proc = false;

    std::ifstream cmdline_file;
    std::string file_contain {};
    std::string cmdline_value {};
    std::ostringstream path;

    struct dirent *dirent;
    DIR* dir;
    dir = opendir(PROC_FOLDER);

    if(dir)
    {
        dirent = readdir(dir);
        while(!pres_proc && dirent)
        {
            if (isdigit(*dirent->d_name))
            {
                path << "/proc/" << dirent->d_name << "/cmdline";
                cmdline_file.open(path.str());
                // TODO: make message in case failed opening of file by log4cplus
                if (cmdline_file.is_open())
                {
                    std::getline(cmdline_file, cmdline_value);
                    if (cmdline_value.find(proc_name) !=  std::string::npos)
                    {
                        pid = dirent->d_name;
                        pres_proc = true;
                    }
                }
                path.clear();
                path.str("");
                cmdline_file.close();
            }
            dirent = readdir(dir);
        }
        closedir(dir);
    }

    return pid;
}

/* получие данных о потреблении ЦП процессом с указанным PID */
/**
 * gets consumption of CPU for process with curenr ID
 * @param pid_c PID
 * @return
 */
int process_cpu(const std::string &pid_c)
{
    int res {-1}; // храню результат сложения utime и stime
    std::string::size_type pos {};
    std::string::size_type initialPos {};
    char ch {' '}; // split line by this character

    std::vector<std::string> stat_values_list;
    std::ifstream stat_file;
    std::ostringstream stat_path; // храню путь к "/path/<pid>/stat"

    std::string stat_value; // значение файла "/path/<pid>/stat"
    std::string getline_res;

    stat_path << "/proc/" << pid_c << "/stat";
    std::cout << stat_path.str() << std::endl;
    stat_file.open(stat_path.str()); // открываю файл
    if (stat_file.is_open())
    {

        std::getline(stat_file, getline_res);
        pos = getline_res.find(ch);
        while( pos != std::string::npos )
        {
            stat_values_list.push_back(getline_res.substr( initialPos, pos - initialPos + 1 ) );
            initialPos = pos + 1;
            pos = getline_res.find( ch, initialPos );
        }

        res = std::stoi(stat_values_list[13]) + std::stoi(stat_values_list[14]);
    }
    else
        res = -1;
    stat_file.close();
    return res;
}

/* получение среза текущего процесорного времени по списку процессов
void get_cpu_section(std::vector <process_info_t> *config, int out_arr[])
{
    for (u_int i = 0; i < config->size() ; ++i)
    {
        out_arr[i] = process_cpu((config->at(i)).pid);
        printf("out_arr %d %s\n", out_arr[i], (config->at(i)).pid);
    }

}*/