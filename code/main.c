#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <locale.h>
#include <stddef.h>

// cpu block
static void read_cpu(unsigned long long *total_out, unsigned long long *idle_out) {
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    unsigned long long irq = 0, softirq = 0, steal = 0;
    FILE *cpufile = fopen("/proc/stat", "r");
    if(!cpufile) return;
    char buf[512];
    if(fgets(buf, sizeof(buf), cpufile))
        sscanf(buf, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(cpufile);
    *total_out = user + nice + system + idle + iowait + irq + softirq + steal;
    *idle_out = idle + iowait;
}

// ram block
struct mem_info {
    unsigned long long total_kB;
    unsigned long long available_kB;
    unsigned long long swap_total_kB;
    unsigned long long swap_free_kB;
};

static void read_mem(struct mem_info *m) {
    FILE *f = fopen("/proc/meminfo", "r");
    if(!f) return;
    char line [256];
    while(fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %llu kB", &m->total_kB);
        sscanf(line, "MemAvailable: %llu kB", &m->available_kB);
        sscanf(line, "SwapTotal: %llu kB", &m->swap_total_kB);
        sscanf(line, "SwapFree: %llu kB", &m->swap_free_kB);
    }
    fclose(f);
}

// temp block
static int find_cpu_temp_path(char *out, size_t out_size) {
    char hwpath[256];
    char hwname[32];
    for(int i = 0; i < 10; i++) {
        snprintf(hwpath, sizeof(hwpath), "/sys/class/hwmon/hwmon%d/name", i);
        FILE *f = fopen(hwpath, "r");
        if(!f) continue;
        hwname[0] = '\0';
        fgets(hwname, sizeof(hwname), f);
        fclose(f);
        if(strncmp(hwname, "coretemp", 8) == 0 ||
                strncmp(hwname, "k10temp", 7) == 0 || 
                strncmp(hwname, "cpu_thermal", 11) == 0) {
            snprintf(out, out_size, "/sys/class/hwmon/hwmon%d/temp1_input", i);
            return 0;
        }
    }
    return -1;
}

static int read_temp(const char *path, float *out) {
    FILE *f = fopen(path, "r");
    if(!f) return -1;
    int raw; 
    int readed = fscanf(f, "%d", &raw);
    fclose(f);
    if(readed != 1) return -1;
    *out = raw / 1000.0f; 
    return 0;
}

int main(void) {
    setlocale(LC_ALL, "");

    // buffers
    char buffer[1024];
    ssize_t bytesRead;

    struct mem_info mem = {0};
    unsigned long long prevtotal0 = 0;
    unsigned long long previdle0 = 0;
    int mode = 0;

    // path to the temp file
    char cputemppath[256] = "";
    char hwpath[256];
    char hwname[32];
    
    /*for(int i = 0; i < 10; i++) {
        snprintf(hwpath, sizeof(hwpath), "/sys/class/hwmon/hwmon%d/name", i);
        FILE *f = fopen(hwpath, "r");
        if(f == NULL) continue;
        hwname[0] = '\0';
        fgets(hwname, sizeof(hwname), f);
        fclose(f);
        if(strncmp(hwname, "coretemp", 8) == 0 ||
                strncmp(hwname, "k10temp", 7) == 0 ||
                strncmp(hwname, "cpu_thermal", 11) == 0) {
            snprintf(cputemppath, sizeof(cputemppath), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            break;
        }
    }*/
    char cpu_temp_path[256] = "";
    find_cpu_temp_path(cputemppath, sizeof(cputemppath));
    

    // ncurses start
    initscr();
    noecho();
    curs_set(0);
    WINDOW *win = newwin(10, 90, 1, 5);
    wtimeout(win, 2000);

    unsigned long long prevtotal = 0, previdle = 0;
    read_cpu(&prevtotal, &previdle);
    napms(200);

    while(1) {
        // temp block
        /*FILE *cputempfile = fopen(cputemppath, "r");
        int temp_readed = 0;
        if(cputempfile != NULL) {
            temp_readed = 1;
            bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, cputempfile); 
            buffer[bytesRead] = '\0';
            int temp = atoi(buffer);
            temp_raw = temp / 1000.0;
            fclose(cputempfile);
        }*/
        float temp_raw = 0.0f;
        int temp_readed = (read_temp(cputemppath, &temp_raw) == 0);

        // ram 
        read_mem(&mem);
        unsigned long long mem_usage_kB = mem.total_kB - mem.available_kB;
        unsigned long long swap_usage_kB = mem.swap_total_kB - mem.swap_free_kB;

        // cpu usage
        unsigned long long total = prevtotal, idle = previdle;
        read_cpu(&total, &idle);
        unsigned long long totald = total - prevtotal;
        unsigned long long idled = idle - previdle;
        float cpu_usage = 0.0f;
        if(totald > 0) {
            cpu_usage = ((float)(totald - idled) / totald) * 100.0f;
        } 
        prevtotal = total;
        previdle = idle;


        // output block
        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, 1, "%-15s %5.1f%%", "CPU LOAD:", cpu_usage);
        if(temp_readed == 1) {
            mvwprintw(win, 2, 1, "%-15s %5.1f°C", "TEMP:", temp_raw);
        }
        else mvwprintw(win, 2, 1, "%-15s %5s", "TEMP:", "n/a");

        const float kb_to_Gb = 1024.0f * 1024.0f;

        if(mode == 1) {
            mvwprintw(win, 3, 1, "%-15s %5.1f MB", "MemTotal:", mem.total_kB / 1024.0f);
            mvwprintw(win, 4, 1, "%-15s %5.1f MB", "MemAvailable:", mem.available_kB / 1024.0f);
            mvwprintw(win, 5, 1, "%-15s %5.1f MB", "MemUsage:", mem_usage_kB / 1024.0f);

            if(mem.swap_total_kB != 0) {
                mvwprintw(win, 6, 1, "%-15s %5.1f MB", "SwapTotal:", mem.swap_total_kB / 1024.0f);
                mvwprintw(win, 7, 1, "%-15s %5.1f MB", "SwapFree:", mem.swap_free_kB / 1024.0f);
                mvwprintw(win, 8, 1, "%-15s %5.1f MB", "SwapUsage:", swap_usage_kB / 1024.0f);
            } 
            else mvwprintw(win, 6, 1, "%-15s %5s", "Swap:", "n/a");
        }
        else { 
            mvwprintw(win, 3, 1, "%-15s %5.1f GB", "MemTotal:", mem.total_kB / kb_to_Gb);
            mvwprintw(win, 4, 1, "%-15s %5.1f GB", "MemAvailable:", mem.available_kB / kb_to_Gb);
            mvwprintw(win, 5, 1, "%-15s %5.1f GB", "MemUsage:", mem_usage_kB / kb_to_Gb);

            if(mem.swap_total_kB != 0) {
                mvwprintw(win, 6, 1, "%-15s %5.1f GB", "SwapTotal:", mem.swap_total_kB / kb_to_Gb);
                mvwprintw(win, 7, 1, "%-15s %5.1f GB", "SwapFree:", mem.swap_free_kB / kb_to_Gb);
                mvwprintw(win, 8, 1, "%-15s %5.1f GB", "SwapUsage:", swap_usage_kB / kb_to_Gb);
            } 
            else mvwprintw(win, 6, 1, "%-15s %5s", "Swap:", "n/a");
        }
        mvprintw(11, 6, "q - exit || g - change output mode");
        wnoutrefresh(stdscr);
        wnoutrefresh(win); 
        doupdate();
        int input = wgetch(win);
        if(input == 'g') mode = !mode;
        else if(input == 'q') break;
    }
    delwin(win);
    endwin();
    return 0; 
}
