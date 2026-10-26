#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#define FALLBACK_COLS (80)
#define FALLBACK_ROWS (24)

#define EMPTY_CHAR (' ')

#define PI (3.14159265358979323846)

#define R1 (1.0)            
#define R2 (2.0)            

#define K2 (5.0)            
#define ASPECT (2.0)        

#define EXTENT (0.75)
#define MARGIN (1.0)

#define SAMPLE_SPACING (0.5)
#define MAX_OOZ (1.0 / (K2 - R1 - R2))

#define A_STEP (0.04)       
#define B_STEP (0.02)       
#define FRAME_MS (30)       

#define LIGHT_X (0.0)
#define LIGHT_Y (1.0)
#define LIGHT_Z (-1.0)
#define L_SCALE (8.0)       

#if defined(_WIN32)
static void platform_init(void) {
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static void sleep_ms(int ms) {
    Sleep((DWORD)ms);
}

static int terminal_size(int *cols, int *rows) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return 0;
    *cols = info.srWindow.Right - info.srWindow.Left + 1;
    *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return 1;
}
#else
static void platform_init(void) {
}

static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int terminal_size(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0 || ws.ws_row == 0)
        return 0;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 1;
}
#endif

static double my_sin(double x) {
    double x2 = x * x;
    double term = x, sum = x;
    for (int n = 1; n < 16; ++n) {
        term *= -x2 / ((2 * n) * (2 * n + 1));
        sum += term;
    }
    return sum;
}

static double my_cos(double x) {
    double x2 = x * x;
    double term = 1.0, sum = 1.0;
    for (int n = 1; n < 16; ++n) {
        term *= -x2 / ((2 * n - 1) * (2 * n));
        sum += term;
    }
    return sum;
}

#define array_init(array, length, value) \
do {                                     \
    for (int i = 0; i < (length); ++i)   \
        (array)[i] = (value);            \
} while(0)                               

#define CLEAR_TERMINAL printf("\x1b[2J")
#define HIDE_CURSOR printf("\x1b[?25l")
#define SHOW_CURSOR printf("\x1b[?25h")
#define ENTER_ALT_SCREEN printf("\x1b[?1049h")
#define LEAVE_ALT_SCREEN printf("\x1b[?1049l")

static volatile sig_atomic_t running = 1;

static void on_signal(int sig) {
    (void)sig;
    running = 0;
}

static char characters[] = ".,-~:;=!*#$@";

int main(void) {
    int cols = 0, rows = 0;
    char *screen_buffer = NULL;
    double *z_buffer = NULL;
    char *frame = NULL;
    
    platform_init();
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    
    ENTER_ALT_SCREEN;
    HIDE_CURSOR;
    
    double A = 0;
    double B = 0;
    
    while (running) {
        int new_cols, new_rows;
        if (!terminal_size(&new_cols, &new_rows)) {
            new_cols = FALLBACK_COLS;
            new_rows = FALLBACK_ROWS;
        }
        
        if (new_cols != cols || new_rows != rows) {
            cols = new_cols;
            rows = new_rows;
            
            free(screen_buffer);
            free(z_buffer);
            free(frame);
            screen_buffer = malloc((size_t)cols * rows);
            z_buffer = malloc((size_t)cols * rows * sizeof *z_buffer);
            frame = malloc((size_t)rows * (cols + 16) + 16);
            if (!screen_buffer || !z_buffer || !frame) {
                fprintf(stderr, "out of memory\n");
                return 1;
            }
            
            CLEAR_TERMINAL;
        }
        
        int cells = cols * rows;
        array_init(screen_buffer, cells, EMPTY_CHAR);
        array_init(z_buffer, cells, 0.0);
        
        double center_x = cols / 2.0;
        double center_y = rows / 2.0;
        
        double k1_x = (center_x - MARGIN) / (EXTENT * ASPECT);
        double k1_y = (center_y - MARGIN) / EXTENT;
        double K1 = k1_x < k1_y ? k1_x : k1_y;
        
        double sin_A = my_sin(A);
        double cos_A = my_cos(A);
        
        double sin_B = my_sin(B);
        double cos_B = my_cos(B);
        
        int theta_count = K1 > 0 ? (int)(2 * PI * K1 * MAX_OOZ * ASPECT * R1 / SAMPLE_SPACING) + 1 : 0;
        double theta_step = theta_count > 0 ? 2 * PI / theta_count : 0.0;
        double sin_dtheta = my_sin(theta_step);
        double cos_dtheta = my_cos(theta_step);
        double sin_theta = 0.0, cos_theta = 1.0;
        
        for (int i = 0; i < theta_count; ++i) {
            double ring_radius = R2 + R1 * cos_theta;
            int phi_count = (int)(2 * PI * K1 * MAX_OOZ * ASPECT * ring_radius / SAMPLE_SPACING) + 1;
            double phi_step = 2 * PI / phi_count;
            double sin_dphi = my_sin(phi_step);
            double cos_dphi = my_cos(phi_step);
            double sin_phi = 0.0, cos_phi = 1.0;
            
            for (int j = 0; j < phi_count; ++j) {

                double x = ring_radius * cos_phi;
                double y = R1 * sin_theta;
                double z = -ring_radius * sin_phi;
                
                double y1 = y * cos_A - z * sin_A;
                double z1 = y * sin_A + z * cos_A;
                
                double x2 = x * cos_B - y1 * sin_B;
                double y2 = x * sin_B + y1 * cos_B;
                
                double ooz = 1 / (z1 + K2);
                int screen_x = (int)(center_x + K1 * ooz * x2 * ASPECT);
                int screen_y = (int)(center_y - K1 * ooz * y2);
                
                if (0 <= screen_x && screen_x < cols && 0 <= screen_y && screen_y < rows) {
                    int index = screen_y * cols + screen_x;
                    
                    if (ooz > z_buffer[index]) {
                        z_buffer[index] = ooz;
                        
                        double nx = cos_theta * cos_phi;
                        double ny = sin_theta;
                        double nz = -cos_theta * sin_phi;
                        
                        double ny1 = ny * cos_A - nz * sin_A;
                        double nz1 = ny * sin_A + nz * cos_A;
                        
                        double nx2 = nx * cos_B - ny1 * sin_B;
                        double ny2 = nx * sin_B + ny1 * cos_B;
                        
                        double L = nx2 * LIGHT_X + ny2 * LIGHT_Y + nz1 * LIGHT_Z;
                        
                        char character = L > 0 ? characters[(int)(L * L_SCALE)] : ' ';
                        screen_buffer[index] = character;
                    }
                }
                
                double next_sin_phi = sin_phi * cos_dphi + cos_phi * sin_dphi;
                cos_phi = cos_phi * cos_dphi - sin_phi * sin_dphi;
                sin_phi = next_sin_phi;
            }
            
            double next_sin_theta = sin_theta * cos_dtheta + cos_theta * sin_dtheta;
            cos_theta = cos_theta * cos_dtheta - sin_theta * sin_dtheta;
            sin_theta = next_sin_theta;
        }

        char *p = frame;
        p += sprintf(p, "\x1b[?25l");
        for (int y = 0; y < rows; ++y) {
            int length = (y == rows - 1) ? cols - 1 : cols;
            p += sprintf(p, "\x1b[%d;1H", y + 1);
            memcpy(p, &screen_buffer[y * cols], (size_t)length);
            p += length;
        }
        fwrite(frame, 1, (size_t)(p - frame), stdout);
        
        fflush(stdout);
        sleep_ms(FRAME_MS);
        
        A += A_STEP;
        if (A >= 2 * PI) A -= 2 * PI;
        
        B += B_STEP;
        if (B >= 2 * PI) B -= 2 * PI;
    }
    
    LEAVE_ALT_SCREEN;
    SHOW_CURSOR;
    fflush(stdout);
    
    free(screen_buffer);
    free(z_buffer);
    free(frame);
    return 0;
}