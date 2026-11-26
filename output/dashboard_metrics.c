#ifdef __linux__

#include "dashboard_metrics.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <limits.h>
#include <linux/inet_diag.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/tcp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

struct process_sample {
    int pid;
    char name[32];
    uint64_t start, ticks, rss, read, written;
    bool io_readable;
};

struct device_sample {
    char name[32];
    uint64_t read, written;
};

struct tcp_sample {
    uint32_t inode;
    uint32_t cookie[2];
    uint64_t bytes, transferred;
};

static struct tcp_sample *previous_tcp;
static size_t previous_tcp_count;
static struct process_sample *previous_processes;
static size_t previous_process_count;
static struct device_sample *previous_disk, *previous_network;
static size_t previous_disk_count, previous_network_count;
struct cpu_reading {
    uint64_t total, idle;
    double load;
    bool seen;
};

static struct cpu_reading aggregate_cpu;
static struct cpu_reading *cpu_cores;
static size_t cpu_capacity, cpu_count;

static uint64_t delta(uint64_t current, uint64_t previous) {
    return current >= previous ? current - previous : 0;
}

static void rank_entry(struct dashboard_metric *metric, const char *name, int pid, double value) {
    for (int i = 0; i < DASHBOARD_TOP_COUNT; i++) {
        struct dashboard_entry *entry = &metric->top[i];
        if (value <= 0 || (value <= entry->value && entry->name[0]))
            continue;

        for (int j = DASHBOARD_TOP_COUNT - 1; j > i; j--)
            metric->top[j] = metric->top[j - 1];
        snprintf(entry->name, sizeof(entry->name), "%s", name);
        entry->pid = pid;
        entry->value = value;
        break;
    }
}

static void update_cpu(struct cpu_reading *reading, uint64_t total, uint64_t idle) {
    uint64_t total_delta = delta(total, reading->total);
    uint64_t idle_delta = delta(idle, reading->idle);
    reading->load = -1;
    if (reading->total && total_delta)
        reading->load =
            idle_delta >= total_delta ? 0 : 100.0 * (1.0 - (double)idle_delta / total_delta);
    reading->total = total;
    reading->idle = idle;
    reading->seen = true;
}

static void sample_cpu(struct dashboard_metric *metric) {
    aggregate_cpu.seen = false;
    cpu_count = 0;
    for (size_t i = 0; i < cpu_capacity; i++)
        cpu_cores[i].seen = false;

    FILE *file = fopen("/proc/stat", "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file) && strncmp(line, "cpu", 3) == 0) {
            char name[32];
            unsigned long long user, nice, system, idle, wait, irq, softirq, steal;
            if (sscanf(line, "%31s %llu %llu %llu %llu %llu %llu %llu %llu", name, &user, &nice,
                       &system, &idle, &wait, &irq, &softirq, &steal) != 9)
                continue;

            uint64_t idle_ticks = idle + wait;
            uint64_t total = user + nice + system + idle_ticks + irq + softirq + steal;
            if (name[3] == '\0') {
                update_cpu(&aggregate_cpu, total, idle_ticks);
                continue;
            }

            size_t index = strtoul(name + 3, NULL, 10);
            if (index >= cpu_capacity) {
                size_t capacity = cpu_capacity ? cpu_capacity * 2 : 16;
                if (capacity <= index)
                    capacity = index + 1;
                struct cpu_reading *cores = realloc(cpu_cores, capacity * sizeof(*cores));
                if (!cores)
                    break;
                memset(cores + cpu_capacity, 0, (capacity - cpu_capacity) * sizeof(*cores));
                cpu_cores = cores;
                cpu_capacity = capacity;
            }
            update_cpu(&cpu_cores[index], total, idle_ticks);
            if (cpu_count <= index)
                cpu_count = index + 1;
        }
        fclose(file);
    }

    for (size_t i = 0; i < cpu_capacity; i++) {
        if (!cpu_cores[i].seen)
            cpu_cores[i] = (struct cpu_reading){0};
    }
    if (aggregate_cpu.seen && aggregate_cpu.load >= 0) {
        metric->ready = true;
        metric->primary = aggregate_cpu.load;
    } else if (!aggregate_cpu.seen) {
        aggregate_cpu.total = 0;
    }
}

size_t dashboard_cpu_count(void) { return cpu_count; }

double dashboard_cpu_load(size_t index) {
    return index < cpu_count && cpu_cores[index].seen ? cpu_cores[index].load : -1;
}

static void sample_memory(struct dashboard_metric *metric) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file)
        return;

    char line[128];
    unsigned long long total = 0, available = 0, swap_total = 0, swap_free = 0;
    bool found_available = false;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemTotal: %llu", &total) == 1)
            continue;
        if (sscanf(line, "MemAvailable: %llu", &available) == 1) {
            found_available = true;
            continue;
        }
        if (sscanf(line, "SwapTotal: %llu", &swap_total) != 1)
            sscanf(line, "SwapFree: %llu", &swap_free);
    }
    fclose(file);
    if (total && found_available) {
        metric->ready = true;
        metric->primary = delta(total, available) * 1024.0;
        metric->secondary = available * 1024.0;
        metric->swap_used = delta(swap_total, swap_free) * 1024.0;
        metric->swap_total = swap_total * 1024.0;
    }
}

static bool read_process(int pid, struct process_sample *process, bool read_io) {
    char path[64], line[4096];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    size_t size = fread(line, 1, sizeof(line) - 1, file);
    fclose(file);
    line[size] = '\0';

    // comm is parenthesized and can itself contain spaces, newlines and parentheses.
    char *name = strchr(line, '(');
    char *end = strrchr(line, ')');
    if (!name || !end || end <= name)
        return false;
    *end = '\0';
    process->pid = pid;
    snprintf(process->name, sizeof(process->name), "%s", name + 1);

    char *save;
    char *field = strtok_r(end + 1, " ", &save);
    for (int i = 3; field && i <= 24; i++, field = strtok_r(NULL, " ", &save)) {
        switch (i) {
        case 14:
        case 15:
            process->ticks += strtoull(field, NULL, 10);
            break;
        case 22:
            process->start = strtoull(field, NULL, 10);
            break;
        case 24: {
            long long pages = strtoll(field, NULL, 10);
            if (pages > 0)
                process->rss = pages;
            break;
        }
        }
    }

    if (read_io) {
        snprintf(path, sizeof(path), "/proc/%d/io", pid);
        file = fopen(path, "r");
        if (file) {
            unsigned long long bytes;
            bool found_read = false, found_written = false;
            while (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "read_bytes: %llu", &bytes) == 1) {
                    process->read = bytes;
                    found_read = true;
                } else if (sscanf(line, "write_bytes: %llu", &bytes) == 1) {
                    process->written = bytes;
                    found_written = true;
                }
            }
            fclose(file);
            process->io_readable = found_read && found_written;
        }
    }
    return true;
}

static int compare_processes(const void *left, const void *right) {
    const struct process_sample *a = left, *b = right;
    return (a->pid > b->pid) - (a->pid < b->pid);
}

static int compare_sockets(const void *left, const void *right) {
    const struct tcp_sample *a = left, *b = right;
    return (a->inode > b->inode) - (a->inode < b->inode);
}

static bool loopback_socket(const struct inet_diag_msg *message) {
    if (message->idiag_family == AF_INET)
        return (ntohl(message->id.idiag_src[0]) >> 24) == 127 ||
               (ntohl(message->id.idiag_dst[0]) >> 24) == 127;

    struct in6_addr source, destination;
    memcpy(&source, message->id.idiag_src, sizeof(source));
    memcpy(&destination, message->id.idiag_dst, sizeof(destination));
    return IN6_IS_ADDR_LOOPBACK(&source) || IN6_IS_ADDR_LOOPBACK(&destination) ||
           (IN6_IS_ADDR_V4MAPPED(&source) && source.s6_addr[12] == 127) ||
           (IN6_IS_ADDR_V4MAPPED(&destination) && destination.s6_addr[12] == 127);
}

static bool read_tcp_family(int fd, int family, struct tcp_sample **samples, size_t *count,
                            size_t *capacity) {
    struct {
        struct nlmsghdr header;
        struct inet_diag_req_v2 query;
    } request = {
        .header = {.nlmsg_len = sizeof(request),
                   .nlmsg_type = SOCK_DIAG_BY_FAMILY,
                   .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
                   .nlmsg_seq = family},
        .query = {.sdiag_family = family,
                  .sdiag_protocol = IPPROTO_TCP,
                  .idiag_ext = 1u << (INET_DIAG_INFO - 1),
                  .idiag_states = UINT32_MAX},
    };
    struct sockaddr_nl kernel = {.nl_family = AF_NETLINK};
    if (sendto(fd, &request, sizeof(request), 0, (struct sockaddr *)&kernel, sizeof(kernel)) < 0)
        return false;

    // Netlink headers are aligned to four bytes. tcp_info fields are copied to aligned storage.
    uint32_t buffer[16384];
    for (;;) {
        struct sockaddr_nl sender;
        socklen_t address_length = sizeof(sender);
        ssize_t size = recvfrom(fd, buffer, sizeof(buffer), MSG_TRUNC, (struct sockaddr *)&sender,
                                &address_length);
        if (size <= 0 || (size_t)size > sizeof(buffer))
            return false;
        if (sender.nl_pid != 0)
            continue;
        int remaining = (int)size;
        for (struct nlmsghdr *header = (void *)buffer; NLMSG_OK(header, remaining);
             header = NLMSG_NEXT(header, remaining)) {
            if (header->nlmsg_seq != (unsigned)family)
                continue;
            if (header->nlmsg_flags & NLM_F_DUMP_INTR || header->nlmsg_type == NLMSG_ERROR)
                return false;
            if (header->nlmsg_type == NLMSG_DONE)
                return true;
            if (header->nlmsg_type != SOCK_DIAG_BY_FAMILY ||
                NLMSG_PAYLOAD(header, 0) < sizeof(struct inet_diag_msg))
                continue;
            const struct inet_diag_msg *message = NLMSG_DATA(header);
            if (!message->idiag_inode || loopback_socket(message))
                continue;
            int attr_size = NLMSG_PAYLOAD(header, sizeof(*message));
            for (struct rtattr *attr = (void *)(message + 1); RTA_OK(attr, attr_size);
                 attr = RTA_NEXT(attr, attr_size)) {
                if (attr->rta_type != INET_DIAG_INFO ||
                    RTA_PAYLOAD(attr) < offsetof(struct tcp_info, tcpi_bytes_received) +
                                            sizeof(((struct tcp_info *)0)->tcpi_bytes_received))
                    continue;

                struct tcp_info info = {0};
                size_t length = RTA_PAYLOAD(attr);
                memcpy(&info, RTA_DATA(attr), length < sizeof(info) ? length : sizeof(info));
                if (*count == *capacity) {
                    size_t next_capacity = *capacity ? *capacity * 2 : 32;
                    struct tcp_sample *next = realloc(*samples, next_capacity * sizeof(*next));
                    if (!next)
                        return false;
                    *samples = next;
                    *capacity = next_capacity;
                }
                (*samples)[(*count)++] = (struct tcp_sample){
                    .inode = message->idiag_inode,
                    .cookie = {message->id.idiag_cookie[0], message->id.idiag_cookie[1]},
                    .bytes = info.tcpi_bytes_acked + info.tcpi_bytes_received,
                };
                break;
            }
        }
    }
}

static bool sample_tcp(void) {
    struct tcp_sample *samples = NULL;
    size_t count = 0, capacity = 0;
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
    if (fd >= 0) {
        struct timeval timeout = {.tv_usec = 30000};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0) {
            if (!read_tcp_family(fd, AF_INET, &samples, &count, &capacity))
                count = 0;
            size_t ipv4_count = count;
            if (!read_tcp_family(fd, AF_INET6, &samples, &count, &capacity))
                count = ipv4_count;
        }
        close(fd);
    }
    if (count)
        qsort(samples, count, sizeof(*samples), compare_sockets);

    bool traffic = false;
    for (size_t i = 0; i < count && previous_tcp_count; i++) {
        struct tcp_sample *sample = &samples[i];
        const struct tcp_sample *old =
            bsearch(sample, previous_tcp, previous_tcp_count, sizeof(*sample), compare_sockets);
        if (old && memcmp(old->cookie, sample->cookie, sizeof(sample->cookie)) == 0) {
            sample->transferred = delta(sample->bytes, old->bytes);
            traffic |= sample->transferred > 0;
        }
    }
    free(previous_tcp);
    previous_tcp = samples;
    previous_tcp_count = count;
    return traffic;
}

static double process_tcp_bytes(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    DIR *dir = opendir(path);
    if (!dir)
        return 0;

    double bytes = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
        char target[64];
        ssize_t length = readlinkat(dirfd(dir), entry->d_name, target, sizeof(target) - 1);
        if (length <= 0)
            continue;
        target[length] = '\0';
        unsigned inode;
        char end;
        if (sscanf(target, "socket:[%u%c", &inode, &end) != 2 || end != ']')
            continue;
        struct tcp_sample key = {.inode = inode};
        struct tcp_sample *sample =
            bsearch(&key, previous_tcp, previous_tcp_count, sizeof(key), compare_sockets);
        if (sample) {
            bytes += sample->transferred;
            // Do not count the same socket again when it has multiple file descriptors.
            sample->transferred = 0;
        }
    }
    closedir(dir);
    return bytes;
}

static void sample_processes(unsigned enabled, double seconds, bool tcp_traffic,
                             struct dashboard_metric *metrics) {
    struct process_sample *processes = NULL;
    size_t count = 0, capacity = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    long ticks_per_second = sysconf(_SC_CLK_TCK);
    bool read_io = enabled & (1u << DASHBOARD_DISK);
    DIR *dir = opendir("/proc");
    if (!dir)
        goto done;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        char *end;
        long pid = strtol(entry->d_name, &end, 10);
        if (*end || pid <= 0 || pid > INT_MAX)
            continue;
        struct process_sample current = {0};
        if (!read_process((int)pid, &current, read_io))
            continue;

        if (tcp_traffic)
            rank_entry(&metrics[DASHBOARD_NETWORK], current.name, current.pid,
                       process_tcp_bytes(current.pid) / seconds);

        if (count == capacity) {
            size_t next_capacity = capacity ? capacity * 2 : 128;
            struct process_sample *next = realloc(processes, next_capacity * sizeof(*next));
            if (!next)
                break;
            processes = next;
            capacity = next_capacity;
        }
        processes[count++] = current;

        if ((enabled & (1u << DASHBOARD_MEMORY)) && page_size > 0)
            rank_entry(&metrics[DASHBOARD_MEMORY], current.name, current.pid,
                       (double)current.rss * page_size);
        if (!previous_process_count || seconds <= 0)
            continue;
        const struct process_sample *old =
            bsearch(&current, previous_processes, previous_process_count, sizeof(current),
                    compare_processes);
        // Linux can reuse a PID between the two snapshots.
        if (!old || old->start != current.start)
            continue;
        if ((enabled & (1u << DASHBOARD_CPU)) && ticks_per_second > 0)
            rank_entry(&metrics[DASHBOARD_CPU], current.name, current.pid,
                       100.0 * delta(current.ticks, old->ticks) / (seconds * ticks_per_second));
        if (current.io_readable && old->io_readable)
            rank_entry(&metrics[DASHBOARD_DISK], current.name, current.pid,
                       (delta(current.read, old->read) + delta(current.written, old->written)) /
                           seconds);
    }
    closedir(dir);
    if (count)
        qsort(processes, count, sizeof(*processes), compare_processes);

done:
    free(previous_processes);
    previous_processes = processes;
    previous_process_count = count;
}

static bool is_whole_disk(const char *name) {
    char path[64];
    // Count physical and virtual machine drives at the device layer, without partitions or
    // stacked device-mapper/RAID devices that would count the same transfer again.
    snprintf(path, sizeof(path), "/sys/block/%s/device", name);
    return access(path, F_OK) == 0;
}

static bool parse_device(char *line, struct device_sample *device, bool network) {
    unsigned long long read, written;
    if (network) {
        char *separator = strrchr(line, ':');
        if (!separator)
            return false;
        *separator = '\0';
        if (sscanf(line, "%31s", device->name) != 1 || strcmp(device->name, "lo") == 0)
            return false;
        if (sscanf(separator + 1, "%llu %*s %*s %*s %*s %*s %*s %*s %llu", &read, &written) != 2)
            return false;
        device->read = read;
        device->written = written;
    } else {
        if (sscanf(line, "%*u %*u %31s %*s %*s %llu %*s %*s %*s %llu", device->name, &read,
                   &written) != 3 ||
            !is_whole_disk(device->name))
            return false;
        // The diskstats ABI uses 512-byte sectors regardless of logical block size.
        device->read = read * 512;
        device->written = written * 512;
    }
    return true;
}

static void sample_devices(struct dashboard_metric *metric, struct device_sample **previous,
                           size_t *previous_count, double seconds, bool network) {
    struct device_sample *devices = NULL;
    size_t count = 0, capacity = 0;
    FILE *file = fopen(network ? "/proc/net/dev" : "/proc/diskstats", "r");
    if (file) {
        char line[512];
        while (fgets(line, sizeof(line), file)) {
            struct device_sample current;
            if (!parse_device(line, &current, network))
                continue;
            if (count == capacity) {
                size_t next_capacity = capacity ? capacity * 2 : 16;
                struct device_sample *next = realloc(devices, next_capacity * sizeof(*next));
                if (!next)
                    break;
                devices = next;
                capacity = next_capacity;
            }
            devices[count++] = current;

            for (size_t i = 0; i < *previous_count && seconds > 0; i++) {
                const struct device_sample *old = &(*previous)[i];
                if (strcmp(current.name, old->name) != 0)
                    continue;
                double read = delta(current.read, old->read) / seconds;
                double written = delta(current.written, old->written) / seconds;
                metric->primary += read;
                metric->secondary += written;
                break;
            }
        }
        metric->ready = seconds > 0 && !ferror(file);
        fclose(file);
    }
    free(*previous);
    *previous = devices;
    *previous_count = count;
}

void dashboard_sample(unsigned enabled, double seconds,
                      struct dashboard_metric metrics[DASHBOARD_PANEL_COUNT]) {
    memset(metrics, 0, DASHBOARD_PANEL_COUNT * sizeof(*metrics));
    if (enabled & (1u << DASHBOARD_CPU))
        sample_cpu(&metrics[DASHBOARD_CPU]);
    else if (cpu_capacity || aggregate_cpu.total) {
        aggregate_cpu = (struct cpu_reading){0};
        free(cpu_cores);
        cpu_cores = NULL;
        cpu_capacity = cpu_count = 0;
    }
    if (enabled & (1u << DASHBOARD_MEMORY))
        sample_memory(&metrics[DASHBOARD_MEMORY]);
    if (enabled & (1u << DASHBOARD_DISK))
        sample_devices(&metrics[DASHBOARD_DISK], &previous_disk, &previous_disk_count, seconds,
                       false);
    else
        previous_disk_count = 0;
    bool tcp_traffic = false;
    if (enabled & (1u << DASHBOARD_NETWORK)) {
        sample_devices(&metrics[DASHBOARD_NETWORK], &previous_network, &previous_network_count,
                       seconds, true);
        tcp_traffic = sample_tcp() && seconds > 0;
    } else {
        previous_network_count = 0;
        previous_tcp_count = 0;
    }
    if (tcp_traffic ||
        (enabled & ((1u << DASHBOARD_CPU) | (1u << DASHBOARD_MEMORY) | (1u << DASHBOARD_DISK))))
        sample_processes(enabled, seconds, tcp_traffic, metrics);
    else
        previous_process_count = 0;
}

void dashboard_reset_metrics(void) {
    free(previous_tcp);
    previous_tcp = NULL;
    previous_tcp_count = 0;
    free(previous_processes);
    previous_processes = NULL;
    previous_process_count = 0;
    free(previous_disk);
    previous_disk = NULL;
    previous_disk_count = 0;
    free(previous_network);
    previous_network = NULL;
    previous_network_count = 0;
    aggregate_cpu = (struct cpu_reading){0};
    free(cpu_cores);
    cpu_cores = NULL;
    cpu_capacity = cpu_count = 0;
}

#endif
