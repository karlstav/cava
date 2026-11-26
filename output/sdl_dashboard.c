#ifdef __linux__

#include "sdl_dashboard.h"
#include "dashboard_metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const panel_names[] = {"Spectrum", "CPU", "Memory", "Disk", "Network"};
static const char *const panel_ids[] = {"spectrum", "cpu", "memory", "disk", "network"};
static const char layout_header[] = "v1\n";
static const int layouts[][2] = {{0, 0}, {1, 0}, {2, 0}, {1, 2}, {2, 2}, {2, 3}};
static enum dashboard_panel panels[DASHBOARD_PANEL_COUNT] = {DASHBOARD_SPECTRUM};
static int panel_count = 1;
static SDL_Rect regions[DASHBOARD_PANEL_COUNT], badges[DASHBOARD_PANEL_COUNT];
static struct dashboard_metric metrics[DASHBOARD_PANEL_COUNT];

enum { HISTORY_SECONDS = 60 };

struct history_point {
    Uint32 time;
    unsigned ready;
    double primary[DASHBOARD_PANEL_COUNT], secondary[DASHBOARD_PANEL_COUNT];
    double *cpu_loads;
    size_t cpu_count;
};

static struct history_point history[HISTORY_SECONDS + 1];
static int history_head, history_count;

static int window_width, window_height;
static bool dirty = true, menu_open, sample_started, layout_loaded;
static Uint32 last_sample;
static SDL_Point menu_origin, pointer;
static SDL_Rect menu;
static int menu_scale, row_height, label_width, slot_width, menu_padding;

static const SDL_Color text_color = {220, 232, 241, 255};
static const SDL_Color muted = {146, 170, 186, 255};
static const SDL_Color green = {104, 238, 140, 255};
static const SDL_Color border = {57, 78, 91, 255};

// Five bits per row, starting with the leftmost pixel in bit 4.
static const unsigned char glyphs[95][7] = {
    ['!' - ' '] = {4, 4, 4, 4, 4, 0, 4},        ['%' - ' '] = {25, 26, 2, 4, 8, 11, 19},
    ['(' - ' '] = {2, 4, 8, 8, 8, 4, 2},        [')' - ' '] = {8, 4, 2, 2, 2, 4, 8},
    ['+' - ' '] = {0, 4, 4, 31, 4, 4, 0},       ['-' - ' '] = {0, 0, 0, 31, 0, 0, 0},
    ['.' - ' '] = {0, 0, 0, 0, 0, 12, 12},      ['/' - ' '] = {1, 2, 2, 4, 8, 8, 16},
    ['0' - ' '] = {14, 17, 19, 21, 25, 17, 14}, ['1' - ' '] = {4, 12, 4, 4, 4, 4, 14},
    ['2' - ' '] = {14, 17, 1, 2, 4, 8, 31},     ['3' - ' '] = {30, 1, 1, 14, 1, 1, 30},
    ['4' - ' '] = {2, 6, 10, 18, 31, 2, 2},     ['5' - ' '] = {31, 16, 16, 30, 1, 1, 30},
    ['6' - ' '] = {14, 16, 16, 30, 17, 17, 14}, ['7' - ' '] = {31, 1, 2, 4, 8, 8, 8},
    ['8' - ' '] = {14, 17, 17, 14, 17, 17, 14}, ['9' - ' '] = {14, 17, 17, 15, 1, 1, 14},
    [':' - ' '] = {0, 12, 12, 0, 12, 12, 0},    ['?' - ' '] = {14, 17, 1, 2, 4, 0, 4},
    ['A' - ' '] = {14, 17, 17, 31, 17, 17, 17}, ['B' - ' '] = {30, 17, 17, 30, 17, 17, 30},
    ['C' - ' '] = {14, 17, 16, 16, 16, 17, 14}, ['D' - ' '] = {30, 17, 17, 17, 17, 17, 30},
    ['E' - ' '] = {31, 16, 16, 30, 16, 16, 31}, ['F' - ' '] = {31, 16, 16, 30, 16, 16, 16},
    ['G' - ' '] = {14, 17, 16, 23, 17, 17, 15}, ['H' - ' '] = {17, 17, 17, 31, 17, 17, 17},
    ['I' - ' '] = {14, 4, 4, 4, 4, 4, 14},      ['J' - ' '] = {7, 2, 2, 2, 2, 18, 12},
    ['K' - ' '] = {17, 18, 20, 24, 20, 18, 17}, ['L' - ' '] = {16, 16, 16, 16, 16, 16, 31},
    ['M' - ' '] = {17, 27, 21, 21, 17, 17, 17}, ['N' - ' '] = {17, 25, 21, 19, 17, 17, 17},
    ['O' - ' '] = {14, 17, 17, 17, 17, 17, 14}, ['P' - ' '] = {30, 17, 17, 30, 16, 16, 16},
    ['Q' - ' '] = {14, 17, 17, 17, 21, 18, 13}, ['R' - ' '] = {30, 17, 17, 30, 20, 18, 17},
    ['S' - ' '] = {15, 16, 16, 14, 1, 1, 30},   ['T' - ' '] = {31, 4, 4, 4, 4, 4, 4},
    ['U' - ' '] = {17, 17, 17, 17, 17, 17, 14}, ['V' - ' '] = {17, 17, 17, 17, 17, 10, 4},
    ['W' - ' '] = {17, 17, 17, 21, 21, 21, 10}, ['X' - ' '] = {17, 17, 10, 4, 10, 17, 17},
    ['Y' - ' '] = {17, 17, 10, 4, 4, 4, 4},     ['Z' - ' '] = {31, 1, 2, 4, 8, 16, 31},
    ['[' - ' '] = {14, 8, 8, 8, 8, 8, 14},      [']' - ' '] = {14, 2, 2, 2, 2, 2, 14},
    ['_' - ' '] = {0, 0, 0, 0, 0, 0, 31},       ['a' - ' '] = {0, 0, 14, 1, 15, 17, 15},
    ['b' - ' '] = {16, 16, 22, 25, 17, 17, 30}, ['c' - ' '] = {0, 0, 14, 17, 16, 17, 14},
    ['d' - ' '] = {1, 1, 13, 19, 17, 17, 15},   ['e' - ' '] = {0, 0, 14, 17, 31, 16, 14},
    ['f' - ' '] = {6, 9, 8, 28, 8, 8, 8},       ['g' - ' '] = {0, 0, 15, 17, 15, 1, 14},
    ['h' - ' '] = {16, 16, 22, 25, 17, 17, 17}, ['i' - ' '] = {4, 0, 12, 4, 4, 4, 14},
    ['j' - ' '] = {2, 0, 6, 2, 2, 18, 12},      ['k' - ' '] = {16, 16, 18, 20, 24, 20, 18},
    ['l' - ' '] = {12, 4, 4, 4, 4, 4, 14},      ['m' - ' '] = {0, 0, 26, 21, 21, 21, 21},
    ['n' - ' '] = {0, 0, 22, 25, 17, 17, 17},   ['o' - ' '] = {0, 0, 14, 17, 17, 17, 14},
    ['p' - ' '] = {0, 0, 30, 17, 30, 16, 16},   ['q' - ' '] = {0, 0, 15, 17, 15, 1, 1},
    ['r' - ' '] = {0, 0, 22, 25, 16, 16, 16},   ['s' - ' '] = {0, 0, 15, 16, 14, 1, 30},
    ['t' - ' '] = {8, 8, 28, 8, 8, 9, 6},       ['u' - ' '] = {0, 0, 17, 17, 17, 19, 13},
    ['v' - ' '] = {0, 0, 17, 17, 17, 10, 4},    ['w' - ' '] = {0, 0, 17, 17, 21, 21, 10},
    ['x' - ' '] = {0, 0, 17, 10, 4, 10, 17},    ['y' - ' '] = {0, 0, 17, 17, 15, 1, 14},
    ['z' - ' '] = {0, 0, 31, 2, 4, 8, 31},
};

static int min_int(int a, int b) { return a < b ? a : b; }

static int clamp_position(int value, int extent, int available) {
    return value < 0 || available < extent ? 0 : min_int(value, available - extent);
}

static void set_color(SDL_Renderer *renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, int width, SDL_Color color,
                      const char *text) {
    set_color(renderer, color);
    for (; *text && width >= 5 * scale; text++, x += 6 * scale, width -= 6 * scale) {
        unsigned char ch = *text;
        if (ch < ' ' || ch > '~')
            ch = '?';
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (glyphs[ch - ' '][row] & (16 >> col)) {
                    SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
}

static int panel_slot(enum dashboard_panel panel) {
    for (int i = 0; i < panel_count; i++) {
        if (panels[i] == panel)
            return i;
    }
    return -1;
}

static unsigned enabled_metrics(void) {
    unsigned enabled = 0;
    for (int i = 0; i < panel_count; i++)
        enabled |= 1u << panels[i];
    return enabled & ~(1u << DASHBOARD_SPECTRUM);
}

static SDL_Rect menu_row(int row) {
    return (SDL_Rect){menu.x + menu_padding, menu.y + menu_padding + (row + 1) * row_height,
                      menu.w - 2 * menu_padding, row_height};
}

static SDL_Rect menu_slot(SDL_Rect row, int slot) {
    return (SDL_Rect){row.x + label_width + slot * slot_width, row.y + 1, slot_width - 2,
                      row.h - 2};
}

static void position_menu(void) {
    menu_scale = window_width >= 300 && window_height >= 156 ? 2 : 1;
    row_height = 11 * menu_scale;
    label_width = 70 * menu_scale;
    slot_width = 14 * menu_scale;
    menu_padding = 4 * menu_scale;
    menu.w = label_width + DASHBOARD_PANEL_COUNT * slot_width + 2 * menu_padding;
    menu.h = (DASHBOARD_PANEL_COUNT + 1) * row_height + 2 * menu_padding;

    int xs[2 * DASHBOARD_PANEL_COUNT + 1] = {menu_origin.x};
    int ys[2 * DASHBOARD_PANEL_COUNT + 1] = {menu_origin.y};
    for (int i = 0; i < panel_count; i++) {
        xs[2 * i + 1] = badges[i].x - menu.w;
        xs[2 * i + 2] = badges[i].x + badges[i].w;
        ys[2 * i + 1] = badges[i].y - menu.h;
        ys[2 * i + 2] = badges[i].y + badges[i].h;
    }

    menu.x = clamp_position(menu_origin.x, menu.w, window_width);
    menu.y = clamp_position(menu_origin.y, menu.h, window_height);
    int best_distance = -1;
    for (int x = 0; x < 2 * panel_count + 1; x++) {
        for (int y = 0; y < 2 * panel_count + 1; y++) {
            SDL_Rect candidate = menu;
            candidate.x = clamp_position(xs[x], menu.w, window_width);
            candidate.y = clamp_position(ys[y], menu.h, window_height);
            bool overlaps = false;
            for (int i = 0; i < panel_count; i++)
                overlaps |= SDL_HasIntersection(&candidate, &badges[i]);
            int distance = abs(candidate.x - menu_origin.x) + abs(candidate.y - menu_origin.y);
            if (!overlaps && (best_distance < 0 || distance < best_distance)) {
                best_distance = distance;
                menu = candidate;
            }
        }
    }
}

static void layout_panels(void) {
    int slot = 0;
    int rows = layouts[panel_count][1] ? 2 : 1;
    for (int row = 0; row < rows; row++) {
        int columns = layouts[panel_count][row];
        int y = row * window_height / rows;
        int height = (row + 1) * window_height / rows - y;
        for (int col = 0; col < columns; col++, slot++) {
            int x = col * window_width / columns;
            int width = (col + 1) * window_width / columns - x;
            regions[slot] = (SDL_Rect){x, y, width, height};
            int badge_width = min_int(25, width);
            badges[slot] = (SDL_Rect){x + width - badge_width, y, badge_width, min_int(25, height)};
        }
    }
    if (menu_open)
        position_menu();
    dirty = true;
}

static double percentage(double used, double total) { return total > 0 ? 100.0 * used / total : 0; }

static void record_history(Uint32 now) {
    struct history_point *point = &history[history_head];
    history_head = (history_head + 1) % (int)SDL_arraysize(history);
    if (history_count < (int)SDL_arraysize(history))
        history_count++;
    free(point->cpu_loads);
    *point = (struct history_point){.time = now};

    for (int panel = DASHBOARD_CPU; panel < DASHBOARD_PANEL_COUNT; panel++) {
        const struct dashboard_metric *metric = &metrics[panel];
        if (!metric->ready)
            continue;
        point->ready |= 1u << panel;
        point->primary[panel] = metric->primary;
        point->secondary[panel] = metric->secondary;
        if (panel == DASHBOARD_MEMORY) {
            point->primary[panel] =
                percentage(metric->primary, metric->primary + metric->secondary);
            point->secondary[panel] = percentage(metric->swap_used, metric->swap_total);
        }
    }

    if (point->ready & (1u << DASHBOARD_CPU)) {
        size_t count = dashboard_cpu_count();
        point->cpu_loads = count ? malloc(count * sizeof(*point->cpu_loads)) : NULL;
        if (point->cpu_loads) {
            point->cpu_count = count;
            for (size_t core = 0; core < count; core++)
                point->cpu_loads[core] = dashboard_cpu_load(core);
        }
    }
}

static const struct history_point *history_point_at(int age) {
    int index =
        (history_head + (int)SDL_arraysize(history) - 1 - age) % (int)SDL_arraysize(history);
    return &history[index];
}

static char *layout_path(void) {
    char *dir = SDL_GetPrefPath("cava", "cava");
    if (!dir)
        return NULL;

    const char filename[] = "dashboard-layout";
    size_t size = strlen(dir) + sizeof(filename);
    char *path = malloc(size);
    if (path)
        snprintf(path, size, "%s%s", dir, filename);
    SDL_free(dir);
    return path;
}

static void load_layout(void) {
    char *path = layout_path();
    FILE *file = path ? fopen(path, "r") : NULL;
    free(path);
    if (!file)
        return;

    enum dashboard_panel saved[DASHBOARD_PANEL_COUNT];
    int count = 0;
    unsigned seen = 0;
    char line[32];
    bool valid = fgets(line, sizeof(line), file) && strcmp(line, layout_header) == 0;
    while (valid && fgets(line, sizeof(line), file)) {
        char *end = strchr(line, '\n');
        if (!end || count == DASHBOARD_PANEL_COUNT) {
            valid = false;
            break;
        }
        *end = '\0';

        int panel = 0;
        while (panel < DASHBOARD_PANEL_COUNT && strcmp(line, panel_ids[panel]) != 0)
            panel++;
        if (panel == DASHBOARD_PANEL_COUNT || (seen & (1u << panel))) {
            valid = false;
            break;
        }
        seen |= 1u << panel;
        saved[count++] = panel;
    }

    if (valid && !ferror(file)) {
        memcpy(panels, saved, count * sizeof(*saved));
        panel_count = count;
    }
    fclose(file);
}

static bool write_layout(const char *path, char *temp_path) {
    int fd = mkstemp(temp_path);
    if (fd < 0)
        return false;

    FILE *file = fdopen(fd, "w");
    bool saved = false;
    if (file) {
        fputs(layout_header, file);
        for (int i = 0; i < panel_count; i++)
            fprintf(file, "%s\n", panel_ids[panels[i]]);
        bool complete = !ferror(file);
        saved = fclose(file) == 0 && complete && rename(temp_path, path) == 0;
    } else {
        close(fd);
    }
    if (!saved)
        unlink(temp_path);
    return saved;
}

static void save_layout(void) {
    char *path = layout_path();
    if (!path) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not locate the dashboard layout file");
        return;
    }

    size_t size = strlen(path) + sizeof(".XXXXXX");
    char *temp_path = malloc(size);
    if (temp_path)
        snprintf(temp_path, size, "%s.XXXXXX", path);
    if (!temp_path || !write_layout(path, temp_path))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not save dashboard layout: %s", path);
    free(temp_path);
    free(path);
}

static void toggle_panel(enum dashboard_panel panel) {
    int slot = panel_slot(panel);
    if (slot < 0) {
        panels[panel_count++] = panel;
    } else {
        panel_count--;
        for (int i = slot; i < panel_count; i++)
            panels[i] = panels[i + 1];
    }
    if (panel != DASHBOARD_SPECTRUM) {
        metrics[panel] = (struct dashboard_metric){0};
        for (size_t i = 0; i < SDL_arraysize(history); i++)
            history[i].ready &= ~(1u << panel);
        if (!enabled_metrics()) {
            dashboard_reset_metrics();
            sample_started = false;
        }
    }
    layout_panels();
    save_layout();
}

bool dashboard_update(SDL_Window *window) {
    if (!layout_loaded) {
        load_layout();
        layout_loaded = true;
    }
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    if (width != window_width || height != window_height) {
        window_width = width;
        window_height = height;
        layout_panels();
    }

    unsigned enabled = enabled_metrics();
    Uint32 now = SDL_GetTicks();
    if (enabled && (!sample_started || now - last_sample >= 1000)) {
        dashboard_sample(enabled, sample_started ? (now - last_sample) / 1000.0 : 0, metrics);
        record_history(now);
        last_sample = now;
        sample_started = true;
        dirty = true;
    }

    bool changed = dirty;
    dirty = false;
    return changed;
}

bool dashboard_handle_event(const SDL_Event *event) {
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT) {
        menu_open = !menu_open;
        menu_origin = pointer = (SDL_Point){event->button.x, event->button.y};
        if (menu_open)
            position_menu();
        dirty = true;
        return true;
    }
    if (!menu_open)
        return false;
    if (event->type == SDL_MOUSEMOTION) {
        pointer = (SDL_Point){event->motion.x, event->motion.y};
        dirty = true;
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        pointer = (SDL_Point){event->button.x, event->button.y};
        if (!SDL_PointInRect(&pointer, &menu)) {
            menu_open = false;
        } else {
            for (int panel = 0; panel < DASHBOARD_PANEL_COUNT; panel++) {
                SDL_Rect row = menu_row(panel);
                if (!SDL_PointInRect(&pointer, &row))
                    continue;

                if (pointer.x < row.x + label_width) {
                    toggle_panel(panel);
                    break;
                }
                int current = panel_slot(panel);
                if (current < 0)
                    break;
                for (int slot = 0; slot < panel_count; slot++) {
                    SDL_Rect target = menu_slot(row, slot);
                    if (slot != current && SDL_PointInRect(&pointer, &target)) {
                        panels[current] = panels[slot];
                        panels[slot] = panel;
                        save_layout();
                        break;
                    }
                }
                break;
            }
        }
        dirty = true;
        return true;
    } else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        menu_open = false;
        dirty = true;
        return true;
    } else if (event->type == SDL_WINDOWEVENT &&
               event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        menu_open = false;
        dirty = true;
    }
    return false;
}

SDL_Rect dashboard_spectrum_rect(void) {
    int slot = panel_slot(DASHBOARD_SPECTRUM);
    return slot >= 0 ? regions[slot] : (SDL_Rect){0};
}

bool dashboard_has_overlay(void) {
    return menu_open || panel_count != 1 || panels[0] != DASHBOARD_SPECTRUM;
}

static void format_bytes(char *buffer, size_t size, double bytes, bool rate) {
    static const char *const units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    unsigned unit = 0;
    while (bytes >= 1024 && unit + 1 < SDL_arraysize(units)) {
        bytes /= 1024;
        unit++;
    }
    snprintf(buffer, size, unit ? "%.1f %s%s" : "%.0f %s%s", bytes, units[unit], rate ? "/s" : "");
}

static const SDL_Color chart_blue = {109, 189, 255, 255};
static const SDL_Color chart_orange = {255, 172, 101, 255};
static const SDL_Color chart_violet = {186, 152, 255, 255};
static const SDL_Color core_colors[] = {
    {122, 176, 236, 255}, {240, 150, 103, 255}, {176, 140, 223, 255},
    {201, 188, 102, 255}, {118, 190, 163, 255}, {226, 130, 169, 255},
};

static bool percent_chart(enum dashboard_panel panel) {
    return panel == DASHBOARD_CPU || panel == DASHBOARD_MEMORY;
}

static void format_axis(char *buffer, size_t size, double value, bool percent) {
    if (percent)
        snprintf(buffer, size, "%.0f%%", value);
    else
        format_bytes(buffer, size, value, true);
}

enum chart_series { CHART_PRIMARY, CHART_SECONDARY, CHART_CPU_CORE };

static void draw_series(SDL_Renderer *renderer, const SDL_Rect *plot, enum dashboard_panel panel,
                        double limit, enum chart_series series, size_t core, SDL_Color color) {
    SDL_Point previous = {0};
    Uint32 previous_time = 0;
    bool connected = false;
    set_color(renderer, color);

    for (int age = history_count - 1; age >= 0; age--) {
        const struct history_point *sample = history_point_at(age);
        Uint32 elapsed = last_sample - sample->time;
        if (elapsed > HISTORY_SECONDS * 1000)
            continue;
        if (!(sample->ready & (1u << panel))) {
            connected = false;
            continue;
        }

        double value;
        if (series == CHART_CPU_CORE)
            value = core < sample->cpu_count ? sample->cpu_loads[core] : -1;
        else
            value = series == CHART_PRIMARY ? sample->primary[panel] : sample->secondary[panel];
        if (value < 0) {
            connected = false;
            continue;
        }

        int x = plot->x + plot->w - 1 -
                (int)((double)elapsed * (plot->w - 1) / (HISTORY_SECONDS * 1000));
        int y = plot->y + plot->h - 1 -
                min_int(plot->h - 1, (int)(value * (plot->h - 1) / limit + 0.5));
        if (connected && sample->time - previous_time < 2500)
            SDL_RenderDrawLine(renderer, previous.x, previous.y, x, y);
        else
            SDL_RenderDrawPoint(renderer, x, y);
        previous = (SDL_Point){x, y};
        previous_time = sample->time;
        connected = true;
    }
}

static void draw_chart(SDL_Renderer *renderer, SDL_Rect area, enum dashboard_panel panel, int scale,
                       SDL_Color primary, SDL_Color secondary) {
    if (area.w < 32 || area.h < 10)
        return;

    bool percent = percent_chart(panel);
    double limit = percent ? 100 : 1024;
    size_t core_count = 0;
    for (int age = 0; age < history_count; age++) {
        const struct history_point *sample = history_point_at(age);
        if (!(sample->ready & (1u << panel)) || last_sample - sample->time > HISTORY_SECONDS * 1000)
            continue;
        if (panel == DASHBOARD_CPU && sample->cpu_count > core_count)
            core_count = sample->cpu_count;
        if (!percent) {
            while (sample->primary[panel] > limit || sample->secondary[panel] > limit)
                limit *= 2;
        }
    }

    char axis_top[24], axis_middle[24], axis_zero[24];
    format_axis(axis_top, sizeof(axis_top), limit, percent);
    format_axis(axis_middle, sizeof(axis_middle), limit / 2, percent);
    format_axis(axis_zero, sizeof(axis_zero), 0, percent);
    int label_chars = (int)SDL_max(strlen(axis_top), strlen(axis_middle));
    int axis_width = (label_chars * 6 + 4) * scale;
    int time_height = 10 * scale;
    SDL_Rect plot = {area.x, area.y, area.w - axis_width, area.h - time_height};
    if (plot.w < 24 || plot.h < 12)
        return;

    fill(renderer, &plot, (SDL_Color){13, 21, 28, 255});
    set_color(renderer, (SDL_Color){43, 61, 72, 255});
    SDL_RenderDrawRect(renderer, &plot);
    for (int vertical = 1; vertical < 6; vertical++) {
        int x = plot.x + vertical * (plot.w - 1) / 6;
        SDL_RenderDrawLine(renderer, x, plot.y, x, plot.y + plot.h - 1);
    }
    for (int horizontal = 1; horizontal < 4; horizontal++) {
        int y = plot.y + horizontal * (plot.h - 1) / 4;
        SDL_RenderDrawLine(renderer, plot.x, y, plot.x + plot.w - 1, y);
    }

    for (size_t core = 0; core < core_count; core++)
        draw_series(renderer, &plot, panel, limit, CHART_CPU_CORE, core,
                    core_colors[core % SDL_arraysize(core_colors)]);
    if (panel != DASHBOARD_CPU)
        draw_series(renderer, &plot, panel, limit, CHART_SECONDARY, 0, secondary);
    draw_series(renderer, &plot, panel, limit, CHART_PRIMARY, 0, primary);

    int axis_x = plot.x + plot.w + 4 * scale;
    int tick_height = 7 * scale;
    draw_text(renderer, axis_x, plot.y, scale, axis_width, muted, axis_top);
    if (plot.h >= 3 * tick_height + 4)
        draw_text(renderer, axis_x, plot.y + (plot.h - tick_height) / 2, scale, axis_width, muted,
                  axis_middle);
    draw_text(renderer, axis_x, plot.y + plot.h - tick_height, scale, axis_width, muted, axis_zero);
    int time_y = plot.y + plot.h + 2 * scale;
    draw_text(renderer, plot.x, time_y, scale, plot.w, muted, "60s");
    if (plot.w >= 70 * scale)
        draw_text(renderer, plot.x + (plot.w - 18 * scale) / 2, time_y, scale, 18 * scale, muted,
                  "30s");
    if (plot.w >= 40 * scale)
        draw_text(renderer, plot.x + plot.w - 18 * scale, time_y, scale, 18 * scale, muted, "Now");
}

static void draw_legend(SDL_Renderer *renderer, int x, int y, int width, int scale, SDL_Color color,
                        const char *label, const char *value) {
    SDL_Rect swatch = {x, y + scale, 5 * scale, 5 * scale};
    fill(renderer, &swatch, color);
    char text[48];
    snprintf(text, sizeof(text), "%s %s", label, value);
    draw_text(renderer, x + 8 * scale, y, scale, width - 8 * scale, text_color, text);
}

static void draw_metric(SDL_Renderer *renderer, const SDL_Rect *rect, enum dashboard_panel panel) {
    const struct dashboard_metric *metric = &metrics[panel];
    int scale = SDL_max(1, min_int(3, min_int(rect->w / 186, rect->h / 112)));
    int pad = 5 * scale;
    int x = rect->x + pad, y = rect->y + pad;
    int width = rect->w - 2 * pad, step = 9 * scale;
    int legend_height = (panel == DASHBOARD_MEMORY ? 2 : 1) * step;
    int top_y = rect->y + rect->h - pad - (DASHBOARD_TOP_COUNT + 1) * step;
    int title_scale = scale + 1;
    int chart_y = y + 7 * title_scale + 3 * scale;
    int chart_height = top_y - legend_height - 8 * scale - chart_y;
    SDL_Color primary =
        panel == DASHBOARD_CPU ? green : panel == DASHBOARD_MEMORY ? chart_violet : chart_blue;
    SDL_Color secondary = panel == DASHBOARD_MEMORY ? green : chart_orange;

    fill(renderer, rect, (SDL_Color){17, 26, 34, 255});
    set_color(renderer, border);
    SDL_RenderDrawRect(renderer, rect);
    SDL_RenderSetClipRect(renderer, rect);
    draw_text(renderer, x, y, title_scale, width - 18, text_color, panel_names[panel]);
    if (chart_height >= 24 * scale)
        draw_chart(renderer, (SDL_Rect){x, chart_y, width, chart_height}, panel, scale, primary,
                   secondary);
    y = chart_height >= 24 * scale ? chart_y + chart_height + 3 * scale : chart_y;

    char value[32], line[48];
    if (!metric->ready) {
        draw_text(renderer, x, y, scale, width, muted, "Awaiting sample");
    } else if (panel == DASHBOARD_CPU) {
        snprintf(value, sizeof(value), "%.1f%%", metric->primary);
        draw_legend(renderer, x, y, width, scale, primary, "Total", value);
        draw_text(renderer, x + width / 2, y, scale, width - width / 2, muted, "Logical CPUs");
    } else {
        bool rate = panel != DASHBOARD_MEMORY;
        format_bytes(value, sizeof(value), metric->primary, rate);
        draw_legend(renderer, x, y, width / 2, scale, primary,
                    panel == DASHBOARD_DISK ? "Read" : panel == DASHBOARD_NETWORK ? "RX" : "RAM",
                    value);
        format_bytes(value, sizeof(value), rate ? metric->secondary : metric->swap_used, rate);
        draw_legend(renderer, x + width / 2, y, width - width / 2, scale, secondary,
                    panel == DASHBOARD_DISK ? "Write" : panel == DASHBOARD_NETWORK ? "TX" : "Swap",
                    value);
        if (panel == DASHBOARD_MEMORY) {
            format_bytes(value, sizeof(value), metric->secondary, false);
            snprintf(line, sizeof(line), "Available %s", value);
            draw_text(renderer, x, y + step, scale, width, muted, line);
        }
    }

    y = SDL_max(top_y, y + legend_height + 4 * scale);
    set_color(renderer, border);
    SDL_RenderDrawLine(renderer, x, y - 3 * scale, x + width - 1, y - 3 * scale);
    draw_text(renderer, x, y, scale, width, muted,
              panel == DASHBOARD_NETWORK ? "Top TCP processes" : "Top processes");
    for (int i = 0; i < DASHBOARD_TOP_COUNT; i++) {
        const struct dashboard_entry *entry = &metric->top[i];
        y += step;
        if (!entry->name[0]) {
            draw_text(renderer, x, y, scale, width, muted, "-");
            continue;
        }
        if (panel == DASHBOARD_CPU)
            snprintf(value, sizeof(value), "%.1f%%", entry->value);
        else
            format_bytes(value, sizeof(value), entry->value, panel != DASHBOARD_MEMORY);
        int value_width = (int)strlen(value) * 6 * scale;
        if (entry->pid)
            snprintf(line, sizeof(line), "%d %s", entry->pid, entry->name);
        else
            snprintf(line, sizeof(line), "%s", entry->name);
        draw_text(renderer, x, y, scale, width - value_width - 6 * scale, text_color, line);
        if (value_width <= width)
            draw_text(renderer, x + width - value_width, y, scale, value_width, text_color, value);
    }
    SDL_RenderSetClipRect(renderer, NULL);
}

static void draw_menu(SDL_Renderer *renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    fill(renderer, NULL, (SDL_Color){0, 0, 0, 155});
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    fill(renderer, &menu, (SDL_Color){24, 36, 46, 255});
    set_color(renderer, border);
    SDL_RenderDrawRect(renderer, &menu);

    int scale = menu_scale;
    int text_y = (row_height - 7 * scale) / 2;
    SDL_Rect header = menu_row(-1);
    draw_text(renderer, header.x, header.y + text_y, scale, label_width, muted, "PANELS");
    draw_text(renderer, header.x + label_width, header.y + text_y, scale, header.w - label_width,
              muted, "REGION");

    for (int panel = 0; panel < DASHBOARD_PANEL_COUNT; panel++) {
        int current = panel_slot(panel);
        SDL_Rect row = menu_row(panel);
        SDL_Rect label = {row.x, row.y, label_width - 2, row.h};
        if (SDL_PointInRect(&pointer, &label))
            fill(renderer, &label, (SDL_Color){44, 63, 76, 255});
        SDL_Rect check = {row.x + scale, row.y + text_y, 7 * scale, 7 * scale};
        set_color(renderer, current >= 0 ? green : muted);
        SDL_RenderDrawRect(renderer, &check);
        if (current >= 0) {
            SDL_Rect mark = {check.x + 2 * scale, check.y + 2 * scale, 3 * scale, 3 * scale};
            fill(renderer, &mark, green);
        }
        draw_text(renderer, row.x + 11 * scale, row.y + text_y, scale, label_width - 11 * scale,
                  text_color, panel_names[panel]);

        if (current < 0)
            continue;
        for (int slot = 0; slot < panel_count; slot++) {
            SDL_Rect item = menu_slot(row, slot);
            bool selected = slot == current;
            if (selected || SDL_PointInRect(&pointer, &item))
                fill(renderer, &item,
                     selected ? (SDL_Color){39, 78, 61, 255} : (SDL_Color){44, 63, 76, 255});
            char number[] = {(char)('1' + slot), '\0'};
            draw_text(renderer, item.x + (item.w - 5 * scale) / 2, row.y + text_y, scale, item.w,
                      selected ? green : text_color, number);
        }
    }
}

void dashboard_draw(SDL_Renderer *renderer) {
    for (int i = 0; i < panel_count; i++) {
        if (panels[i] != DASHBOARD_SPECTRUM)
            draw_metric(renderer, &regions[i], panels[i]);
    }
    if (panel_count == 0)
        draw_text(renderer, 10, 10, 2, window_width - 20, muted, "Right-click to enable panels");
    if (!menu_open)
        return;

    draw_menu(renderer);
    for (int i = 0; i < panel_count; i++) {
        if (badges[i].w < 10 || badges[i].h < 14)
            continue;
        char number[] = {(char)('1' + i), '\0'};
        SDL_Rect badge = badges[i];
        badge.x += 1;
        badge.y += 1;
        badge.w -= 2;
        badge.h -= 2;
        fill(renderer, &badge, (SDL_Color){15, 28, 24, 255});
        draw_text(renderer, badges[i].x + (badges[i].w - 10) / 2,
                  badges[i].y + (badges[i].h - 14) / 2, 2, badges[i].w, green, number);
    }
}

void dashboard_close(void) {
    dashboard_reset_metrics();
    memset(metrics, 0, sizeof(metrics));
    for (size_t i = 0; i < SDL_arraysize(history); i++)
        free(history[i].cpu_loads);
    memset(history, 0, sizeof(history));
    history_count = history_head = 0;
    sample_started = false;
    menu_open = false;
    window_width = window_height = 0;
    dirty = true;
}

#endif
