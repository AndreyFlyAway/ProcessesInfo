/**
 * @file process_info.cpp
 * @brief вывод данных по процессу - потребляемая память, процессорное время
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

std::vector <proc_info> process_arr;
std::vector <process_info_t> g_processes_data;

Logger logger;

/* для хранения настроек */
int g_n_freq = 0; // количество интервалов 
int g_sleep_discr = 0; // время между интервалами
int g_average_n = 0; // количество интервалов, которые будет учитываться для вычисления среднего значение
int average_counter = 0; // для подсчте среднего значения
float cpu_load = 1.1;
long g_total_mem; /* для хранения количества памяти на машине */

/* list of file in /proc/sys/kernel/, they are used for reading information about distributive */
const std::string sys_info_list[SYS_FILE_NUM] = {"/proc/sys/kernel/ostype",
                                                     "/proc/sys/kernel/hostname",
                                                     "/proc/sys/kernel/osrelease",
                                                     "/proc/sys/kernel/version", };

int main(int argc, char * argv[])
{
    /* настройка log4cplus */
    log4cplus::Initializer initializer;
    //log4cplus::BasicConfigurator config;
    //config.configure();
    logger = log4cplus::Logger::getInstance(LOGGER_NAME);
    tstring file = LOG4CPLUS_C_STR_TO_TSTRING(LOG4CPLUS_CONFIG_NAME);
    FileInfo fi;

    /* проверка верности конфиг-файла для log4cplus */
    if (getFileInfo (&fi, file) == 0)
    {
        PropertyConfigurator::doConfigure(file);
    }
    else
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong config file for log4cplus!")
                << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong config file for log4cplus!\n";
        return -1;
    }

    /* парсинг аргументов */
    std::string conf_file_path;

    if (argc == 2)
        conf_file_path = argv[1];
    else if (argc == 1)
        conf_file_path = DEFAULT_CONFIG_FILE_PATH;
    else if (argc > 2)
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong argument format: Too many arguments!")
                        << "Exmaple: ./CpuInfo config_processes.ini" << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong argument format: Too many arguments!\nExmaple: ./CpuInfo config_processes.ini\n" ;
        return -1;
    }

    /* загрузка конфигурации с настройками периодов и списокм процессов */
    if (load_config(&g_processes_data, &conf_file_path) == -1)
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong config file with processes list! Get use standart ini-file - config_processes.ini") << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong config file with processes list! Get use standart ini-file - config_processes.ini\n";
        return -1;
    }
    get_total_mem();
}


int main1(int argc, char * argv[])
{
    /* настройка log4cplus */
    log4cplus::Initializer initializer;
    //log4cplus::BasicConfigurator config;
    //config.configure();
    logger = log4cplus::Logger::getInstance(LOGGER_NAME);
    tstring file = LOG4CPLUS_C_STR_TO_TSTRING(LOG4CPLUS_CONFIG_NAME);
    FileInfo fi;

    /* проверка верности конфиг-файла для log4cplus */
    if (getFileInfo (&fi, file) == 0)
    {
        PropertyConfigurator::doConfigure(file);
    }
    else
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong config file for log4cplus!")
                << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong config file for log4cplus!\n";
        return -1;
    }

    /* парсинг аргументов */
    char *conf_file_path;

    if (argc == 2)
        conf_file_path = argv[1];
    else if (argc == 1)
        conf_file_path = (char *)DEFAULT_CONFIG_FILE_PATH;
    else if (argc > 2)
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong argument format: Too many arguments!")
                << "Exmaple: ./CpuInfo config_processes.ini" << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong argument format: Too many arguments!\nExmaple: ./CpuInfo config_processes.ini\n" ;
        return -1;
    }

    /* загрузка конфигурации с настройками периодов и списокм процессов */
    if (load_config(&process_arr, conf_file_path) == -1)
    {
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Wrong config file with processes list! Get use standart ini-file - config_processes.ini") << std::endl);
        /* дублирую в stdout для наглядности */
        std::cout << "Wrong config file with processes list! Get use standart ini-file - config_processes.ini\n";
        return -1;
    }
    /* беру общее количество памят на машине, для вычисление потребления памяти в процентах */
    g_total_mem = get_total_mem();
    LOG4CPLUS_INFO(logger, "Start logging");
    /* Получаю данные по сборки дистрибутива */
    char distr_info[MAXLEN];
    get_system_info(distr_info);
    LOG4CPLUS_INFO_FMT(logger, "Dist version: %s ", distr_info);

    /* первая инициализация PID'ов */
    for (u_int i = 0; i < process_arr.size() ; i++)
        get_pid(process_arr[i].name, process_arr[i].pid);
    /* опрос */
    while(true)
    {
        resources_usage(&process_arr, g_n_freq, g_sleep_discr);
        LOG4CPLUS_INFO_FMT(logger, "Total CPU: %06.2f  ", cpu_load);
        for (u_int i = 0; i < process_arr.size() ; i++)
            LOG4CPLUS_INFO_FMT(logger, "%-18s PID %-6s CPU: %06.2f MEM: %06.2f VIRT %7d RSS: %7d CPU MAX: %06.2f CPU Average: %06.2f", \
			process_arr[i].name, process_arr[i].pid, process_arr[i].cur_CPU_usage, process_arr[i].mem_in_percent, \
			process_arr[i].virt_mem, process_arr[i].rss_mem, process_arr[i].max_CPU_usage,  process_arr[i].average_CPU);
    }

    return 1;
}

/* получение ID процесса по его имени, значение PID сохранятеся в буфер pid, а функция возрващет 1 или -1
если удалось найти процесс */
int get_pid(char* proc_name, char *pid)
{
    struct dirent *dirent;
    int	functiom_res = -1;
    DIR* dir;
    char path[MAXLEN];
    bool pres_proc = false;
    strncpy(pid, "None", PID_LEN);
    dir = opendir(PROC_FOLDER);

    if(dir)
    {
        dirent = readdir(dir);
        while(!pres_proc && dirent)
        {
            if (isdigit(*dirent->d_name))
            {
                sprintf(path, "/proc/%s/cmdline", dirent->d_name);
                //printf("path %s\n", path);
                FILE *proc_file;

                proc_file = fopen(path, "r");
                if (proc_file)
                {
                    char cmdline_value[256];
                    char *istr;

                    fgets(cmdline_value, 128, proc_file);
                    //printf("cmdline_value %s\n", cmdline_value);
                    istr = strstr(cmdline_value, proc_name) ;
                    if (istr != NULL)
                    {
                        //printf("found %s\n", dirent->d_name);
                        memcpy(pid, dirent->d_name, PID_LEN);
                        pres_proc = true;
                        functiom_res = 1;
                    }
                    fclose(proc_file);
                }
            }
            dirent = readdir(dir);
        }
        closedir(dir);
    }
    if (functiom_res == -1)
        memcpy(pid, "None", PID_LEN);
    return functiom_res;
}

/* вычисление текущей загруженности процессора */
int proc_stat(int *cpu_total, int *idle)
//int proc_stat(void)
{
    *cpu_total = 0;	// собираю все данные из stat с преобразованием в float
    bool stop_search = false; // флаг прекращения цикла поиска
    FILE *proc_file; // ссылка на файл
    char proc_stat_value[512]; // для хранени данных из файла
    char procstat_list [30][256]; // для хранения результатов, полученных из /proc/stat
    proc_file = fopen(PROC_STAT_FOLDER, "r"); // указываю файл

    if (proc_file)
    {
        /* вытаскиваю данные из файла */
        char *getline_res; // для хранения результата функции fgetsyy
        char *find_res; // храню результат поиска
        char *word_found; // найденное слово при резульате разделения
        getline_res = fgets(proc_stat_value, 100, proc_file);
        while ( (getline_res != NULL) && !stop_search) // буду читать файла пока не дойду до конча или не попаду в if
        {

            find_res = strstr(proc_stat_value, "cpu"); // ищу нужную строку

            if (find_res != NULL) // если строка найдена, то провожу парсинг
            {
                /* произвожу парсинг */
                word_found = strtok(getline_res, " ");
                //int i = 0;
                for (int i = 0; (i < 30) && (word_found != NULL); i++)
                {
                    /* ставлю копирование до функции поиска, чтобы не копировать NULL, которое функция
                    strtok записывает при завершении поиска */
                    memcpy(procstat_list[i], word_found, strlen(word_found));
                    word_found = strtok(NULL, " ");
                    //printf("word found %d %s \n", i, word_found);
                }
                break;
            }
            getline_res = fgets(proc_stat_value, 256, proc_file);
        }
    }
    fclose(proc_file);
    for(int i=1; i <=8; ++i)
    {
        *cpu_total += atoi(procstat_list[i]);
        //printf(" %s %d\n",  procstat_list[i], i);
    }
    //printf(" %s\n", procstat_list[8]);
    *idle = atoi(procstat_list[4]);
    return *cpu_total;
}

/* получеие данных о потреблении ЦП процессом с указанным PID */
int process_cpu(char *pid_c)
{
    int res = -1; // храню результат сложения utime и stime
    FILE *stat_file;
    char stat_path[MAXLEN]; // храню путь к "/path/<pid>/stat"
    char stat_value[MAXLEN]; // значение файла "/path/<pid>/stat"
    char stat_values_list [30][256];
    char *word_found; // найденное слово в результате разделения


    snprintf(stat_path, MAXLEN, "/proc/%s/stat", pid_c); // получаю полный путь к файлу stat
    //printf("stat path: %s: \n", stat_path);
    /* вытскиваю нужные значение из /proc/<pid>/cmdline */
    stat_file = fopen(stat_path, "r"); // открываю файл
    if (stat_file)
    {
        char *getline_res = fgets(stat_value, MAXLEN, stat_file); // беру строку с данными
        word_found = strtok(getline_res, " ");
        int i = 0;
        //printf("proc pid stat val: %s: \n", stat_value);
        for (i = 0; (i < 20) && (word_found != NULL); i++)
        {
            /* ставлю копирование до функции поиска, чтобы не копировать NULL, который функция
            strtok записывает при завершении поиска */
            memset(stat_values_list[i], 0, sizeof(stat_values_list[i])); // так зачищаю предыдущее значение, тк при копирование может помешать
            memcpy(stat_values_list[i], word_found, strlen(word_found));
            word_found = strtok(NULL, " ");
            //printf("word found %d %s \n", i, word_found);
        }
        fclose(stat_file);
        res = atoi(stat_values_list[13]) + atoi(stat_values_list[14]);
        //printf("MOON %d %s %s \n", res, stat_values_list[13], stat_values_list[14]);
    }
    else
        res = -1;
    return res;
}

/* выичление среднего значения занятого процессрного времени в процентах за период времени */
int resources_usage(std::vector <proc_info> *config, int n_freq, int sleep_t_ms) {
    int res {0};
    int proc_num = process_arr.size(); // количество процессов
    struct proc_info *cur_conf;
    /* для значения процессора */
    int cpu_total {0};
    int pr_cpu_total {0};
    /* для значение процесса */
    int proc_val[proc_num];
    int pr_proc_val[proc_num];
    int proc_res[proc_num];
    float res_cpu_cur = 0;
    /* для значение памяти */
    int virt_mem[proc_num];
    int rss_mem[proc_num];
    float rss_in_percent[proc_num];
    int res_virt[proc_num];
    int res_rss[proc_num];
    float res_rss_percent[proc_num];
    /* для вычиcления полной загруженности */
    int cpu_idle = 0;
    int pr_cpu_idle = 0;
    //float cpu_load = 0.0;
    int diff_idle = 0; // для сопутстyвующих вычислений
    int diff_total = 0; // для сопутстyвующих вычислений

    cpu_load = 0;

    /* инициализация массива */
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
    proc_stat(&pr_cpu_total, &pr_cpu_idle);
    usleep(sleep_t_ms);
    for (int i = 0; i < n_freq; ++i) {
        /* беру первый срез по процесСАМ */
        get_cpu_section(config, proc_val);
        get_mem_section(config, virt_mem, rss_mem, rss_in_percent);
        //printf("out_arr mem %d %f\n", virt_mem[i], rss_in_percent[i]);
        proc_stat(&cpu_total, &cpu_idle);
        for (int j = 0; j < proc_num; ++j) {
            cur_conf = &(config->at(j));
            /* не делаю в данном условии поверку всех значений памяти, т.к. они все беруться из одного метода,
            и, следовательно, из одного открытого файла */
            if (not((proc_res[j] < 0) || (rss_mem[j] <= 0))) {
                res_cpu_cur =
                        (((float(proc_val[j])) - (float(pr_proc_val[j])))) / (float(cpu_total) - float(pr_cpu_total)) *
                        100;
                proc_res[j] += res_cpu_cur;
                res_virt[j] += virt_mem[j];
                res_rss[j] += rss_mem[j];
                res_rss_percent[j] += rss_in_percent[j];
                if ((res_cpu_cur) > (cur_conf->max_CPU_usage))
                    cur_conf->max_CPU_usage = res_cpu_cur;
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
    for (int j = 0; j < proc_num; ++j) {
        cur_conf = &(config->at(j));
        if (proc_res[j] < 0 || (rss_mem[j] <= 0)) {
            get_pid(cur_conf->name, (cur_conf->pid));
            cur_conf->cur_CPU_usage = -1;
            cur_conf->virt_mem = -1;
            cur_conf->rss_mem = -1;
            cur_conf->mem_in_percent = -1;
        } else {
            cur_conf->cur_CPU_usage = round(100 * proc_res[j] / float(n_freq)) / 100;
            cur_conf->virt_mem = res_virt[j] / n_freq;
            cur_conf->rss_mem = res_rss[j] / n_freq;
            cur_conf->mem_in_percent = round(100 * res_rss_percent[j] / float(n_freq)) / 100;
            if ((cur_conf->cur_CPU_usage) > (cur_conf->max_CPU_usage))
                cur_conf->max_CPU_usage = cur_conf->cur_CPU_usage;
            /* вычисления среднего значения  */
            if (average_counter == g_average_n) {
                /* произвожу вычисления среднего значения */
                cur_conf->average_CPU = cur_conf->sum_CPU_values / g_average_n;
                cur_conf->sum_CPU_values = 0;
            } else {
                /* сумирую значение */
                cur_conf->sum_CPU_values += cur_conf->cur_CPU_usage;
            }

        }
    }
    /* вычисляю всю загруженность */
    cpu_load = cpu_load / float(n_freq);
    /*увеличение счетчика для подсчета среднего значения*/
    if (average_counter == g_average_n)
    {
        average_counter = 0;
        res = 1;
    }
    else
    {
        average_counter++;
        res = 0;

    }
    return res;
}

/* получение среза текущего процесорного времени по списку процессов */
void get_cpu_section(std::vector <proc_info> *config, int out_arr[])
{
    for (u_int i = 0; i < config->size() ; ++i)
    {
        out_arr[i] = process_cpu((config->at(i)).pid);
        //printf("out_arr %d %s\n", out_arr[i], (config->at(i)).pid);
    }

}

/* получение среза текущего потребления памяти (виртуальной и резидентной) по списку процессов */
void get_mem_section(std::vector <proc_info> *config, int o_virt_arr[], int o_rss_arr[], float o_rss_perc_arr[])
{
    for (u_int i = 0; i < config->size() ; ++i)
    {
        MEM_usage((config->at(i)).pid, &o_virt_arr[i], &o_rss_arr[i], &o_rss_perc_arr[i]);
        //printf("out_arr mem %d %d %d %f\n ", i, o_virt_arr[i], o_rss_arr[i], o_rss_perc_arr[i]);
    }
}

/* вычисление количества используемой памяти  */
int MEM_usage(char *pid, int *virt_out, int *rss_out, float *rss_percentage)
{
    int f_res = -1; // храню результат сложения utime и stime
    FILE *status_file;
    char status_path[MAXLEN]; // храню путь к "/path/<pid>/stat"
    char status_value[MAXLEN]; // значение файла "/path/<pid>/stat"

    if (strstr(pid, "None"))
    {
        *virt_out = -1;
        *rss_out = -1;
        return f_res;
    }

    snprintf(status_path, MAXLEN, "/proc/%s/status", pid); // получаю полный путь к файлу stat
    status_file = fopen(status_path, "r"); // открываю файл

    if (status_file)
    {
        char *istr_VmRSS;
        char *istr_VmSize;
        char text_buf[100];
        float rss_perc_temp;
        for (int i = 0; i < STATUS_FILE_LINES; i++)
        {
            fgets(status_value, MAXLEN, status_file); // беру строку с данными
            istr_VmSize = strstr(status_value, "VmSize");
            istr_VmRSS = strstr(status_value, "VmRSS") ;
            /* копирую точные значение виртуальной и резидентной памяти */
            if (istr_VmSize != NULL)
            {
                copy_digit_words(status_value, text_buf);
                *virt_out = atoi(text_buf);
            }
            if (istr_VmRSS != NULL)
            {
                copy_digit_words(status_value, text_buf);
                *rss_out = atoi(text_buf);
                /* вычисляю значения резидентой памяти в процентах, то значение памяти в %, которое показывает htop */
                rss_perc_temp = (float(*rss_out) / float(g_total_mem)) * 100;
                *rss_percentage = round(rss_perc_temp*100) / 100;
                //printf("MEM kek %d %s\n", *rss_out, *rss_percentage);
            }
        }
        f_res = 1;
        fclose(status_file);
    }
    else
    {
        *virt_out = 0;
        *rss_out = 0;
        *rss_percentage = 0;
        f_res = -1;
    }
    return f_res;
}

/* копирование первого вхождения слова с цифрами в строке */
int copy_digit_words(const std::string * str, std::string * buf)
{
    int res;
    int j = 0;
    for (auto i = 0 : *str)
    {
        if (isdigit(i))
            std::cout << i << std::endl;
            //*buf.append(i);
    }
    buf[j] = '\0';

    if (j > 0)
        res = 1;
    else
        res = -1;

    return res;
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
int load_config(std::vector <process_info_t> *config, const std::string * conf_file_path)
{
    INIReader reader(*conf_file_path);
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
        std::cout << temp_val.name << std::endl;
        pos1 = pos2 + 1;
        config->push_back(temp_val);
        temp_val.name.clear();
    }
    /* хоть и при неверном ini-файое значения у цифирных переменных будут -1, все равно сделал услвоие проверки
    х > 0, дабы защититься от неверных настроек  */
    if ((config->size() == 0) || !(g_n_freq > 0) || !(g_sleep_discr > 0) || !(g_average_n > 0) )
        return 0;

    return 1;
}

template<typename T, typename P>
T remove_if(T beg, T end, P pred)
{
    T dest = beg;
    for (T itr = beg;itr != end; ++itr)
        if (!pred(*itr))
            *(dest++) = *itr;
    return dest;
}

/* копирование слова до конца строки */
int copy_word(char * str_in, char * str_out)
{
    int i = 0;
    /* решетка добавлена для возомжности добавления комментариев в конце строки */
    while (str_in[i] != '\n')
    {
        str_out[i] = str_in[i];
        ++i;
    }
    str_out[i] = '\0';
    return 1;
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
        res = get_digit_word()

    }
    else
    {
        LOG4CPLUS_INFO_FMT(logger, "Cant read %s",  MEMINFO_FOLDER);
    };

    meminfo_file.close();
    return res;
//    if (meminfo_file)
//    {
//        fgets(meminfo_value, 100, meminfo_file);
//        copy_digit_words(meminfo_value, buf);
//        fclose(meminfo_file);
//    }
//    return atoi(buf);
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

