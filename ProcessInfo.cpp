/**
 * @file process_info.cpp
 * @brief вывод данных по процессу - потребляемая память, процессорное время (файл для рефакторинга)
 * @author Yanikeev-AS
 * @version 0.1
 * @date 2018-06-27
 */

#include "ProcessInfo.h"
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

//std::vector <proc_info> process_arr;
std::vector <process_info::process_info_t> g_processes_data;

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

int main(int argc, char * argv[]) {
    using namespace std;
    using namespace process_info;

    /* Testing load_config */
    string str{"kek 12345"};
    string buf{};
    load_config(g_processes_data, DEFAULT_CONFIG_FILE_PATH);
    int out_arr[g_processes_data.size()];
    for (u_int i = 0; i < g_processes_data.size(); i++) {
        g_processes_data[i].pid = get_pid(g_processes_data[i].name);
    }

    g_total_mem = get_total_mem();

    for(int i = 0 ; i < 40 ; i++)
    {
        resources_usage(g_processes_data, 3, 300000);
        for (u_int i = 0; i < g_processes_data.size() ; i++)
            cout << g_processes_data[i].name
                 << " PID: " << g_processes_data[i].pid
                 << " CPU: " << g_processes_data[i].cur_CPU_usage
                 << " MEM: " << g_processes_data[i].mem_in_percent
                 << " VRS: " <<  g_processes_data[i].virt_mem
                 << " RSS: " <<  g_processes_data[i].rss_mem
                 << " CPU Average: " <<  g_processes_data[i].average_CPU
                 << endl;
    }

}

namespace process_info {

    /* получение общего количества памяти на машине */
    /**
     *  gets total memory that machine has
     * @return total memrory as std::string object, or text "None" if reading /proc/meminfo is faild
     */
    long get_total_mem(void) {
        using namespace std;
        ifstream meminfo_file;
        string meminfo_value{};
        long res{};

        meminfo_file.open(MEMINFO_FOLDER);
        if (meminfo_file.is_open()) {
            getline(meminfo_file, meminfo_value);
            res = stoi(get_num_frm_str(meminfo_value));

        } else {
            LOG4CPLUS_INFO_FMT(logger, "Cant read %s", MEMINFO_FOLDER);
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
    int get_system_info(char *str_out) {
        std::ifstream info_file;
        std::string file_contain{};
        int copy_pos{1};
        int res{};

        for (auto file_name : sys_info_list) {
            info_file.open(file_name);
            if (info_file.is_open()) {
                info_file >> file_contain;
                file_contain.append(" ");
                file_contain.copy(str_out + copy_pos, file_contain.size());
                copy_pos += file_contain.size();
            } else
                res += 1;
            info_file.close();

        }
        return res;
    }


    /* копирование первого вхождения слова с цифрами в строке */
    /**
     *
     *  copies the first sequences of numbers in a string
     *
     * @param str copy from
     * @param buf copy to
     * @return number of copied characters
     */
    std::string get_num_frm_str(const std::string &str) {
        std::size_t const n = str.find_first_of("0123456789");
        if (n != std::string::npos) {
            std::size_t const m = str.find_first_not_of("0123456789", n);
            return str.substr(n, m != std::string::npos ? m - n : m);
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
    int load_config(std::vector<process_info_t> &config, const std::string &conf_file_path) {
        INIReader reader(conf_file_path);
        struct process_info_t temp_val{};
        int pos1{};
        int pos2{};

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
        while (pos2 != -1) {
            pos2 = processes.find(",", pos1);
//        processes.copy(temp_val.name, pos2-pos1, pos1);
            temp_val.name = processes.substr(pos1, pos2 - pos1);
//        std::cout << temp_val.name << std::endl;
            pos1 = pos2 + 1;
            config.push_back(temp_val);
            temp_val.name.clear();
        }
        /* хоть и при неверном ini-файое значения у цифирных переменных будут -1, все равно сделал услвоие проверки
        х > 0, дабы защититься от неверных настроек  */
        if ((config.size() == 0) || !(g_n_freq > 0) || !(g_sleep_discr > 0) || !(g_average_n > 0))
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
    std::string get_pid(const std::string &proc_name) {
        std::string pid{"None"};
        bool pres_proc = false;

        std::ifstream cmdline_file;
        std::string file_contain{};
        std::string cmdline_value{};
        std::ostringstream path;

        struct dirent *dirent;
        DIR *dir;
        dir = opendir(PROC_FOLDER);

        if (dir) {
            dirent = readdir(dir);
            while (!pres_proc && dirent) {
                if (isdigit(*dirent->d_name)) {
                    path << "/proc/" << dirent->d_name << "/cmdline";
                    cmdline_file.open(path.str());
                    // TODO: make message in case failed opening of file by log4cplus
                    if (cmdline_file.is_open()) {
                        std::getline(cmdline_file, cmdline_value);
                        if (cmdline_value.find(proc_name) != std::string::npos) {
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
    int process_cpu(const std::string &pid_c) {
        int res{-1}; // храню результат сложения utime и stime

        std::vector<std::string> stat_values_list;
        std::ifstream stat_file;
        std::ostringstream stat_path; // храню путь к "/path/<pid>/stat"

        std::string getline_res;

        stat_path << "/proc/" << pid_c << "/stat";
        stat_file.open(stat_path.str()); // открываю файл
        if (stat_file.is_open()) {

            std::getline(stat_file, getline_res);
            split_string(getline_res, stat_values_list);
            res = std::stoi(stat_values_list[13]) + std::stoi(stat_values_list[14]);
        } else
            res = -1;
        stat_file.close();
        return res;
    }

    /* получение среза текущего процесорного времени по списку процессов */
    /**
     * gets consumption of CPU for all processes
     * @param config config for each process
     * @param out_arr buffer where will be saved information
     */
    void get_cpu_section(std::vector<process_info_t> &config, int out_arr[]) {
        for (u_int i = 0; i < config.size(); ++i) {
            // TODO: investigate why I used at(i)
            out_arr[i] = process_cpu((config.at(i)).pid);
        }

    }

    /* вычисление текущей загруженности процессора */
    /**
     * calculates currnet CPU consumption
     * @param cpu_total
     * @param idle
     * @return
     */
    int &proc_stat(int &cpu_total, int &idle) {
        using namespace std;

        cpu_total = 0;    // собираю все данные из stat с преобразованием в float
        bool stop_search = false; // флаг прекращения цикла поиска

        string::size_type pos{};
        string::size_type word_found_pos{};
        string::size_type initialPos{};

        ifstream proc_file;
        string proc_stat_value; // для хранени данных из файла
        vector<string> procstat_list; // для хранения результатов, полученных из /proc/stat

        proc_file.open(PROC_STAT_FOLDER); // jnrhsdf. файл
        if (proc_file.is_open()) {
            /* вытаскиваю данные из файла */
            getline(proc_file, proc_stat_value);

            while (getline(proc_file, proc_stat_value) &&
                   !stop_search) // буду читать файла пока не дойду до конча или не попаду в if
            {
                pos = proc_stat_value.find("cpu"); // ищу нужную строку
                if (pos != string::npos) // если строка найдена, то провожу парсинг
                {
                    /* произвожу парсинг */
                    split_string(proc_stat_value, procstat_list);

                    break;
                }
//            getline(proc_file, proc_stat_value);
            }
        }
        proc_file.close();
        for (int i = 2; i <= 9; ++i) {
            cpu_total += stoi(procstat_list[i]);
        }
        idle = stoi(procstat_list[5]);
        return cpu_total;
    }

    /**
     * makes a list of the words in the string, separated by a delimiter string.
     * @param str a needed string
     * @param buf a vector where result will be saved
     * @param sp_str a delimiter string
     * @return reference to the result vector
     */
    std::vector<std::string> &
    split_string(const std::string &str, std::vector<std::string> &buf, const std::string &sp_str) {
        using namespace std;

        string::size_type pos{};
        string::size_type initialPos{};

        pos = str.find(sp_str);
        while (pos != string::npos) {
            buf.push_back(str.substr(initialPos, pos - initialPos));
            initialPos = pos + 1;
            pos = str.find(sp_str, initialPos);
        }
        buf.push_back(str.substr(initialPos, str.size() - 1));

        return buf;
    }


    /* вычисление количества используемой памяти  */
    /**
     * calculates current virtual memory, RSS memory in kb and RSS in percentage
     * @param pid
     * @param virt_out
     * @param rss_out
     * @param rss_percentage
     * @return the success result code
     */
    int MEM_usage(const std::string &pid, int &virt_out, int &rss_out, float &rss_percentage) {
        using namespace std;

        int f_res = -1; // храню результат сложения utime и stime
        ifstream status_file;
        string status_value{}; // значение файла "/path/<pid>/stat"
        string text_buf{};
        float rss_perc_temp{};

        if (pid == "None") {
            virt_out = -1;
            rss_out = -1;
            return f_res;
        }

        ostringstream status_path;

        status_path << "/proc/" << pid << "/status"; // получаю полный путь к файлу stat
        status_file.open(status_path.str());

        if (status_file.is_open()) {
            while (getline(status_file, status_value)) {
                if (status_value.find("VmSize") != string::npos) {
                    text_buf = get_num_frm_str(status_value);
                    virt_out = stoi(text_buf);
                }
                if (status_value.find("VmRSS") != string::npos) {
                    text_buf = get_num_frm_str(status_value);
                    rss_out = stoi(text_buf);
                    /* вычисляю значения резидентой памяти в процентах, то значение памяти в %, которое показывает htop */
                    rss_perc_temp = (float(rss_out) / float(g_total_mem)) * 100;
                    rss_percentage = round(rss_perc_temp * 100) / 100;
                }
            }
            f_res = 1;
        } else {
            virt_out = -1;
            rss_out = -1;
            rss_percentage = -1;
            f_res = -1;
        }
        status_file.close();
        return f_res;
    }

    /* получение среза текущего потребления памяти (виртуальной и резидентной) по списку процессов */
    /**
     * get current memory consumption for all process by their PID
     * @param config config vector, where the names and pids of processes are saved
     * @param o_virt_arr virtual memmry
     * @param o_rss_arr resident memory
     * @param o_rss_perc_arr memory in percentage
     */
    void
    get_mem_section(std::vector<process_info_t> &config, int o_virt_arr[], int o_rss_arr[], float o_rss_perc_arr[]) {
        for (u_int i = 0; i < config.size(); ++i) {
            MEM_usage((config.at(i)).pid, o_virt_arr[i], o_rss_arr[i], o_rss_perc_arr[i]);
//        std::cout << "PID: " << (config.at(i)).pid << " " << (config.at(i)).name
//                                                   << " VIRT: " << o_virt_arr[i]
//                                                   << " RSS: " << o_rss_arr[i]
//                                                   << " PERCENTEGE: " << o_rss_perc_arr[i] << std::endl;
        }
    }

/* выичление среднего значения занятого процессрного времени в процентах за период времени */
    int resources_usage(std::vector<process_info_t> &config, const int n_freq, const int sleep_t_ms) {
        using namespace std;
        int res{0};
        int proc_num{(int) config.size()}; // количество процессов
//    struct process_info_t *cur_conf;
        /* для значения процессора */
        int cpu_total{0};
        int pr_cpu_total{0};
        /* для значение процесса */
        int proc_val[proc_num];
        int pr_proc_val[proc_num];
        int proc_res[proc_num];
        float res_cpu_cur{0};
        /* для значение памяти */
        int virt_mem[proc_num];
        int rss_mem[proc_num];
        float rss_in_percent[proc_num];
        int res_virt[proc_num];
        int res_rss[proc_num];
        float res_rss_percent[proc_num];
        /* для вычиcления полной загруженности */
        int cpu_idle{0};
        int pr_cpu_idle{0};
        //float cpu_load = 0.0;
        int diff_idle{0}; // для сопутстyвующих вычислений
        int diff_total{0}; // для сопутстyвующих вычислений

        cpu_load = 0;

        for (int j = 0; j < proc_num; ++j) {
            virt_mem[j] = 0;
            rss_mem[j] = 0;
            proc_res[j] = 0;
            res_virt[j] = 0;
            res_rss[j] = 0;
            res_rss_percent[j] = 0;
        }

        /* беру самый первый срез */
        get_cpu_section(config, pr_proc_val);
        proc_stat(pr_cpu_total, pr_cpu_idle);
        usleep(sleep_t_ms);
        for (int i = 0; i < n_freq; ++i) {
            /* беру первый срез по процесСАМ */
            get_cpu_section(config, proc_val);
            get_mem_section(config, virt_mem, rss_mem, rss_in_percent);
            //printf("out_arr mem %d %f\n", virt_mem[i], rss_in_percent[i]);
            proc_stat(cpu_total, cpu_idle);

            for (int j = 0; j < proc_num; ++j) {
//            cur_conf = &(config.at(j));
                /* не делаю в данном условии поверку всех значений памяти, т.к. они все беруться из одного метода,
                и, следовательно, из одного открытого файла */
                if (not((proc_res[j] < 0) || (rss_mem[j] <= 0))) {
                    if (cpu_total == pr_cpu_total) {

                        res_cpu_cur =
                                (((float(proc_val[j])) - (float(pr_proc_val[j])))) /
                                (float(cpu_total) - float(pr_cpu_total)) *
                                100;
                        proc_res[j] += res_cpu_cur;

                    }
                    res_virt[j] += virt_mem[j];
                    res_rss[j] += rss_mem[j];
                    res_rss_percent[j] += rss_in_percent[j];
                    if ((res_cpu_cur) > (config[j].max_CPU_usage))
                        config[j].max_CPU_usage = res_cpu_cur;

                }
            }
            /* вычисления загруженности по процеCСОРУ */
            diff_idle = cpu_idle - pr_cpu_idle;
            diff_total = cpu_total - pr_cpu_total;
            if (diff_total != 0) {
                cpu_load += (100 * (float(diff_total) - float(diff_idle)) / float(diff_total));
            } else {
                cpu_load += 0.0;
            }
            /* сохраняю первый срез по процесСАМ как второй срез и тоже по процесСОРУ*/
            memcpy(pr_proc_val, proc_val, sizeof(proc_val));
            pr_cpu_total = cpu_total;
            pr_cpu_idle = cpu_idle;
            usleep(sleep_t_ms);

        }

        /* копирую средние значения параметров в структуру процесса */
        for (int j = 0; j < proc_num; j++) {
//        cur_conf = &(config->at(j));
            if (proc_res[j] < 0 || (rss_mem[j] <= 0)) {
                config[j].pid = get_pid(config[j].name);
                config[j].cur_CPU_usage = -1;
                config[j].virt_mem = -1;
                config[j].rss_mem = -1;
                config[j].mem_in_percent = -1;
            } else {

                config[j].cur_CPU_usage = round(100 * proc_res[j] / float(n_freq)) / 100;
                config[j].virt_mem = res_virt[j] / n_freq;
                config[j].rss_mem = res_rss[j] / n_freq;
                config[j].mem_in_percent = round(100 * res_rss_percent[j] / float(n_freq)) / 100;

                if ((config[j].cur_CPU_usage) > (config[j].max_CPU_usage))
                    config[j].max_CPU_usage = config[j].cur_CPU_usage;
                /* вычисления среднего значения  */
                if (average_counter == g_average_n) {
                    /* произвожу вычисления среднего значения */
                    config[j].average_CPU = config[j].sum_CPU_values / g_average_n;
                    config[j].sum_CPU_values = 0;
                } else {
                    /* сумирую значение */
                    config[j].sum_CPU_values += config[j].cur_CPU_usage;
                }

            }

        }
        /* вычисляю всю загруженность */
        cpu_load = cpu_load / float(n_freq);
        cout << "cpu_load " << cpu_load << endl;
        /*увеличение счетчика для подсчета среднего значения*/

        if (average_counter == g_average_n) {
            average_counter = 0;
            res = 1;
        } else {
            average_counter++;
            res = 0;

        }

        return res;
    }
}