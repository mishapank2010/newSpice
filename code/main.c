#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <locale.h>

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

int main(void) {
    setlocale(LC_ALL, "");

    // bufera (.)(.)
    char buffer[1024];
    char line[256];
    ssize_t bytesRead;
    unsigned long long prevtotal0 = 0;
    unsigned long long previdle0 = 0;
    unsigned long long mem_total_kB = 0;
    unsigned long long mem_available_kB = 0;
    unsigned long long swap_total_kB = 0;
    unsigned long long swap_free_kB = 0; 
    float temp_raw;

    // path to the temp file
    char cputemppath[256] = "";
    char hwpath[256];
    char hwname[32];
    
    for(int i = 0; i < 10; i++) {
        snprintf(hwpath, sizeof(hwpath), "/sys/class/hwmon/hwmon%d/name", i);
        FILE *zalupus = fopen(hwpath, "r");
        if(zalupus == NULL) continue;
    hwname[0] = '\0';
    fgets(hwname, sizeof(hwname), zalupus);
    fclose(zalupus);
    if(strncmp(hwname, "coretemp", 8) == 0 ||
            strncmp(hwname, "k10temp", 7) == 0 ||
            strncmp(hwname, "cpu_thermal", 11) == 0) {
            snprintf(cputemppath, sizeof(cputemppath), "/sys/class/hwmon/hwmon%d/temp1_input", i);
            break;
            }
    }
    
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
        FILE *cputempfile = fopen(cputemppath, "r");
        if(cputempfile != NULL) {
            bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, cputempfile); 
            buffer[bytesRead] = '\0';
            int temp = atoi(buffer);
            temp_raw = temp / 1000.0;
            fclose(cputempfile);
        }

        // ram block
        FILE *ramfile = fopen("/proc/meminfo", "r");
        if(ramfile != NULL) {
            while(fgets(line, sizeof(line), ramfile)) {
                sscanf(line, "MemTotal: %llu kB", &mem_total_kB);
                sscanf(line, "MemAvailable: %llu kB", &mem_available_kB);
                sscanf(line, "SwapTotal: %llu kB", &swap_total_kB);
                sscanf(line, "SwapFree: %llu kB", &swap_free_kB);
            }
            fclose(ramfile);
        }
        unsigned long long mem_usage_kB = mem_total_kB - mem_available_kB;
        unsigned long long swap_usage_kB = swap_total_kB - swap_free_kB;

        // yobani proccessor
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
        mvwprintw(win, 1, 1, "CPU LOAD: %.1f%%", cpu_usage);
        if(cputempfile != NULL) {
            mvwprintw(win, 2, 1, "TEMP: %.1f°C", temp_raw);
        }
        else mvwprintw(win, 2, 1, "TEMP: n/a");

        mvwprintw(win, 3, 1, "MemTotal: %.1f MB", mem_total_kB / 1024.0f);
        mvwprintw(win, 4, 1, "MemAvailable: %.1f MB", mem_available_kB / 1024.0f);
        mvwprintw(win, 5, 1, "MemUsage: %.1f MB", mem_usage_kB / 1024.0f);

        if(swap_total_kB != 0) {
            mvwprintw(win, 6, 1, "SwapTotal: %.1f MB", swap_total_kB / 1024.0f);
            mvwprintw(win, 7, 1, "SwapFree: %.1f MB", swap_free_kB / 1024.0f);
            mvwprintw(win, 8, 1, "SwapUsage: %.1f MB", swap_usage_kB / 1024.0f);
        } 
        else mvwprintw(win, 6, 1, "Swap: n/a");
        wrefresh(win);
        if(wgetch(win) == 'q') break;
    }
    delwin(win);
    endwin();
    return 0; 
}
