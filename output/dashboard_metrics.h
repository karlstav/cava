#ifndef DASHBOARD_METRICS_H
#define DASHBOARD_METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum dashboard_panel {
    DASHBOARD_SPECTRUM,
    DASHBOARD_CPU,
    DASHBOARD_MEMORY,
    DASHBOARD_DISK,
    DASHBOARD_NETWORK,
    DASHBOARD_PANEL_COUNT
};

enum { DASHBOARD_TOP_COUNT = 3 };

struct dashboard_entry {
    char name[32];
    int pid;
    double value;
};

struct dashboard_metric {
    bool ready;
    double primary;
    double secondary;
    double swap_used, swap_total;
    struct dashboard_entry top[DASHBOARD_TOP_COUNT];
};

void dashboard_sample(unsigned enabled, double seconds,
                      struct dashboard_metric metrics[DASHBOARD_PANEL_COUNT]);
size_t dashboard_cpu_count(void);
double dashboard_cpu_load(size_t index);
void dashboard_reset_metrics(void);

#endif
