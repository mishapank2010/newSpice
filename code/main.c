#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>

// #define clear() printf("\033[H\033[J") // ctrl + l func (no longer used due ncurses)

int main() {
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
    while(1) {
        sleep(2);
        clear();

        // file opening 
        FILE *cputempfile = fopen(cputemppath, "r");
        FILE *cpuloadfile = fopen("/proc/stat", "r");
        FILE *ramfile = fopen("/proc/meminfo", "r");

        // temp block
        if(cputempfile != NULL) {
            bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, cputempfile); 
            buffer[bytesRead] = '\0';
            int temp = atoi(buffer);
            temp_raw = temp / 1000.0;
            fclose(cputempfile);
        }
        

        // ram block
        if(ramfile != NULL) {
            while(fgets(line, sizeof(line), ramfile)) {
                sscanf(line, "MemTotal: %llu kB", &mem_total_kB);
                sscanf(line, "MemAvailable: %llu kB", &mem_available_kB);
                sscanf(line, "SwapTotal: %llu kB", &swap_total_kB);
                sscanf(line, "SwapFree: %llu kB", &swap_free_kB);
            }
        }
        
        int mem_usage_kB = mem_total_kB - mem_available_kB;
        int swap_usage_kB = swap_total_kB - swap_free_kB;
        fclose(ramfile);

        // yobani proccessor

        unsigned long long user0 = 0, nice0 = 0, system0 = 0, idle0 = 0, iowait0 = 0;
        unsigned long long irq0 = 0, softirq0 = 0, steal0 = 0;
        
        if(fgets(buffer, sizeof(buffer), cpuloadfile) != NULL) {
            sscanf(buffer, "%*s %llu %llu %llu %llu %llu %llu %llu %llu", 
                &user0, &nice0, &system0, &idle0, &iowait0, &irq0, &softirq0, &steal0); 
        unsigned long long total0 = user0 + nice0 + system0 + idle0 + iowait0 + irq0 + softirq0 + steal0;
        unsigned long long idle_total0 = idle0 + iowait0;

        unsigned long long totald0 = total0 - prevtotal0;
        unsigned long long idled0 = idle_total0 - previdle0;

        float cpu_usage = 0.0f;
        if(totald0 > 0) {
            cpu_usage = ((float)(totald0 - idled0) / totald0) * 100.0f;
        } 
        prevtotal0 = total0;
        previdle0 = idle_total0;

        // output block
        initscr();
        noecho();
        //keypad(stdscr, TRUE); 
        int height, widht, start_y, start_x;
        height = 10;
        widht = 90;
        start_y = 1;
        start_x = 5;

        WINDOW * win = newwin(height, widht, start_y, start_x);
        refresh();
        
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
        }
    }
    endwin();
    return 0; 
}
