#include <string.h>
#include <time.h>
#include <locale.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>

#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <alsa/asoundlib.h>
#include <fftw3.h>

#ifdef __GNUC__
// curses.h or other sources may already define
#undef GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

static const int M = 2048;
static int shared[2048];
static int format = -1;
static unsigned int rate = 0;

static bool scientificMode = false;

static struct termios oldtio, newtio;
static int rc;

void cleanup(void);
void sig_handler(int sig_no);
void *input_alsa(void *data);
void *input_fifo(void *data);

// general: cleanup
void cleanup()
{
    printf("\033[0m\n");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
    system("setterm -cursor on");
    system("setterm -blank 10");
    system("clear");
    rc = tcsetattr(0, TCSAFLUSH, &oldtio);
}

// general: handle signals
void sig_handler(int sig_no)
{
    cleanup();
    if (sig_no == SIGINT)
        printf("CTRL-C pressed -- goodbye\n");
    signal(sig_no, SIG_DFL);
    raise(sig_no);
}

// input: ALSA
void* input_alsa(void* data)
{
    const char *device = ((char *)data);
    snd_pcm_t *handle;
    int err = 0;

    // alsa: open device to capture audio
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0)) {
        cleanup();
        fprintf(stderr,
                "error opening stream: %s\n",
                snd_strerror(err));
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    else
        printf("open stream successful\n");
#endif

    int dir;
    snd_pcm_hw_params_t *params;

    // assembling params
    snd_pcm_hw_params_alloca(&params);

    // setting defaults or something
    snd_pcm_hw_params_any(handle, params);

    // interleaved mode right left right left
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // trying to set 16bit
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

    // assuming stereo
    snd_pcm_hw_params_set_channels(handle, params, 2);

    // trying 44100 rate
    unsigned int val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    // number of frames per read
    snd_pcm_uframes_t frames = 256;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    // attempting to set params
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        cleanup();
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *)&val);

    // converting result to number of bits
    if (val < 6)
        format = 16;
    else if (val > 5 && val < 10)
        format = 24;
    else if (val > 9)
        format = 32;

    // getting rate
    snd_pcm_hw_params_get_rate(params, &rate, &dir);

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    snd_pcm_hw_params_get_period_time(params, &val, &dir);

    // frames * bits/8 * 2 channels
    const unsigned long size = frames * ((unsigned long)format / 8) * 2;
    signed char *buffer = malloc(size);

    int o = 0;

    while (true) {
        err = (int)snd_pcm_readi(handle, buffer, frames);

        if (err == -EPIPE) {
/* EPIPE means overrun */
#ifdef DEBUG
            fprintf(stderr, "overrun occurred\n");
#endif
            snd_pcm_prepare(handle);
        } else if (err < 0) {
#ifdef DEBUG
            fprintf(stderr, "error from read: %s\n", snd_strerror(err));
#endif
        } else if (err != (int)frames) {
#ifdef DEBUG
            fprintf(stderr, "short read, read %d %d frames\n", err, (int)frames);
#endif
        }

        // adjustments for interleaved
        int radj = format / 4;
        int ladj = format / 8;

        // sorting out one channel and only biggest octet
        for (int i = 0; i < (int)size; i = i + (ladj)*2) {
            // first channel
            // using the 10 upper bits this would give me a vert res of 1024, enough...
            int tempr = ((buffer[i + (radj)-1] << 2));

            int lo = ((buffer[i + (radj)-2] >> 6));
            if (lo < 0)
                lo = abs(lo) + 1;
            if (tempr >= 0)
                tempr = tempr + lo;
            if (tempr < 0)
                tempr = tempr - lo;

            //other channel
            int templ = (buffer[i + (ladj)-1] << 2);
            lo = (buffer[i + (ladj)-2] >> 6);
            if (lo < 0)
                lo = abs(lo) + 1;
            if (templ >= 0)
                templ = templ + lo;
            else
                templ = templ - lo;

            //adding channels and storing it in the buffer
            shared[o] = (tempr + templ) / 2;
            o++;
            if (o == M - 1)
                o = 0;
        }
    }
}

//input: FIFO
void *input_fifo(void* data)
{
    signed char buf[1024];
    const char *path = ((char *)data);

    const int fd = open(path, O_RDONLY);
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int t = 0;
    while (true) {
        const long bytes = read(fd, buf, sizeof(buf));
        const int size = 1024;

        //if no bytes read sleep 10ms and zero shared buffer
        if (bytes == -1) {
            struct timespec req = {.tv_sec = 0, .tv_nsec = 10000000};
            nanosleep(&req, NULL);
            t++;
            if (t > 10)
                for (int i = 0; i < M; i++)
                    shared[i] = 0;
            t = 0;

            //if bytes read go ahead
        } else {
            t = 0;
            int n = 0;

            for (int i = 0; i < (size / 4); i++) {
                int tempr = (buf[4 * i - 1] << 2);
                int lo = (buf[4 * i] >> 6);
                if (lo < 0)
                    lo = abs(lo) + 1;
                if (tempr >= 0)
                    tempr = tempr + lo;
                else
                    tempr = tempr - lo;

                int templ = (buf[4 * i - 3] << 2);
                lo = (buf[4 * i - 2] >> 6);
                if (lo < 0)
                    lo = abs(lo) + 1;
                if (templ >= 0)
                    templ = templ + lo;
                else
                    templ = templ - lo;

                shared[n] = (tempr + templ) / 2;
                n++;
                if (n == M - 1)
                    n = 0;
            }
        }
    }
}

// general: entry point
int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    char *usage = "\n\
Usage : " PACKAGE " [options]\n\
Visualize audio input in terminal. \n\
\n\
Options:\n\
	-b 1..(console columns/2-1) or 200	number of bars in the spectrum (default 25 + fills up the console), program will automatically adjust if there are too many frequency bands)\n\
	-i 'input method'			method used for listening to audio, supports: 'alsa' and 'fifo'\n\
	-o 'output method'			method used for outputting processed data, only supports 'terminal'\n\
	-d 'alsa device'			name of alsa capture device (default 'hw:1,1')\n\
	-p 'fifo path'				path to fifo (default '/tmp/mpd.fifo')\n\
	-c foreground color			supported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)\n\
	-C background color			supported colors: same as above (default: no change)\n\
	-s sensitivity				sensitivity percentage, 0% - no response, 50% - half, 100% - normal, etc...\n\
	-f framerate 				FPS limit, if you are experiencing high CPU usage, try reducing this (default: 60)\n\
	-S					\"scientific\" mode (disables most smoothing)\n\
	-h					print the usage\n\
	-v					print version\n\
\n";

    char *inputMethod = "alsa";
    char *outputMethod = "terminal";
    int om = 1;
    char *device = "hw:1,1";
    char *path = "/tmp/mpd.fifo";

    int fixedbands;
    int autoband = 1;
    struct winsize w;

    char *color;
    int col = 36;
    int bgcol = 0;
    int sens = 100;

    int flast[200];
    int fall[200];
    float fpeak[200];
    float fmem[200];

    for (int i = 0; i < 200; i++) {
        flast[i] = 0;
        fall[i] = 0;
        fpeak[i] = 0;
        fmem[i] = 0;
    }
    for (int i = 0; i < M; i++)
        shared[i] = 0;

    int im = 1;
    int framerate = 60;
    int c = 0;

    // general: handle command-line arguments
    while ((c = getopt(argc, argv, "p:i:b:d:s:f:c:C:hSv")) != -1)
        switch (c) {
            // argument: fifo path
            case 'p':
                path = optarg;
                break;

            // argument: input method
            case 'i':
                im = 0;
                inputMethod = optarg;
                if (strcmp(inputMethod, "alsa") == 0)
                    im = 1;
                if (strcmp(inputMethod, "fifo") == 0)
                    im = 2;
                if (im == 0) {
                    cleanup();
                    fprintf(stderr,
                            "input method %s is not supported, supported methods are: 'alsa' and 'fifo'\n",
                            inputMethod);
                    exit(EXIT_FAILURE);
                }
                break;

            // argument: output method
            case 'o':
                om = 0;
                outputMethod = optarg;
                if (strcmp(outputMethod, "terminal") == 0)
                    im = 1;
                if (im == 0) {
                    cleanup();
                    fprintf(stderr,
                            "output method %s is not supported, supported methods are: 'terminal'\n",
                            outputMethod);
                    exit(EXIT_FAILURE);
                }
                break;

            // argument: bar count
            case 'b':
                fixedbands = atoi(optarg);
                autoband = 0;
                if (fixedbands > 200)
                    fixedbands = 200;
                break;

            // argument: alsa device
            case 'd':
                device = optarg;
                break;

            // argument: sensitivity
            case 's':
                sens = atoi(optarg);
                break;

            // argument: framerate
            case 'f':
                framerate = atoi(optarg);
                break;

            // argument: foreground color
            case 'c':
                col = 0;
                color = optarg;
                if (strcmp(color, "black") == 0)
                    col = 30;
                if (strcmp(color, "red") == 0)
                    col = 31;
                if (strcmp(color, "green") == 0)
                    col = 32;
                if (strcmp(color, "yellow") == 0)
                    col = 33;
                if (strcmp(color, "blue") == 0)
                    col = 34;
                if (strcmp(color, "magenta") == 0)
                    col = 35;
                if (strcmp(color, "cyan") == 0)
                    col = 36;
                if (strcmp(color, "white") == 0)
                    col = 37;
                if (col == 0) {
                    cleanup();
                    fprintf(stderr, "color %s not supported\n", color);
                    exit(EXIT_FAILURE);
                }
                break;

            // argument: background color
            case 'C':
                bgcol = 0;
                color = optarg;
                if (strcmp(color, "black") == 0)
                    bgcol = 40;
                if (strcmp(color, "red") == 0)
                    bgcol = 41;
                if (strcmp(color, "green") == 0)
                    bgcol = 42;
                if (strcmp(color, "yellow") == 0)
                    bgcol = 43;
                if (strcmp(color, "blue") == 0)
                    bgcol = 44;
                if (strcmp(color, "magenta") == 0)
                    bgcol = 45;
                if (strcmp(color, "cyan") == 0)
                    bgcol = 46;
                if (strcmp(color, "white") == 0)
                    bgcol = 47;
                if (bgcol == 0) {
                    cleanup();
                    fprintf(stderr, "color %s not supported\n", color);
                    exit(EXIT_FAILURE);
                }
                break;

            // argument: enable "scientific" mode
            case 'S':
                scientificMode = true;
                break;

            // argument: print usage
            case 'h':
                printf("%s", usage);
                return 0;

            // argument: print usage
            case '?':
                printf("%s", usage);
                return 1;

            // argument: print version
            case 'v':
                printf(PACKAGE " " VERSION "\n");
                return 0;

            // argument: no arguments; exit
            default:
                abort();
        }

    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    rc = tcgetattr(0, &oldtio);
    memcpy(&newtio, &oldtio, sizeof(newtio));
    newtio.c_lflag &= ~(ICANON | ECHO);
    newtio.c_cc[VMIN] = 0;
    rc = tcsetattr(0, TCSAFLUSH, &newtio);

    int thr_id GCC_UNUSED;
    struct timespec req = {.tv_sec = 0, .tv_nsec = 0};

    // input: wait for the input to be ready
    if (im == 1) {
        int n = 0;
        pthread_t p_thread;

        //starting alsamusic listener
        thr_id = pthread_create(&p_thread, NULL, input_alsa, (void *)device);
        while (format == -1 || rate == 0) {
            req.tv_sec = 0;
            req.tv_nsec = 1000000;
            nanosleep(&req, NULL);
            n++;
            if (n > 2000) {
#ifdef DEBUG
                cleanup();
                fprintf(stderr,
                        "could not get rate and/or format, problems with audio thread? quiting...\n");
#endif
                exit(EXIT_FAILURE);
            }
        }

#ifdef DEBUG
        printf("got format: %d and rate %d\n", format, rate);
#endif
    }

    if (im == 2) {
        pthread_t p_thread;
        //starting fifomusic listener
        thr_id = pthread_create(&p_thread, NULL, input_fifo, (void *)path);
        rate = 44100;
        format = 16;
    }

    // 2 * (M / 2 + 1) = 2050
    double in[2050];

    // M / 2 + 1 = 1025
    fftw_complex out[1025][2];

    //planning to rock
    fftw_plan p = fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE);

    // output: get terminal's geometry
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    //jumbing back to this loop means that you resized the screen
    while (true) {
        int bands = 25;

        //getting orignial numbers of bands incase of resize
        if (autoband == 1)
            bands = 25;
        else
            bands = fixedbands;

        if (bands > (int)w.ws_col / 2 - 1)
            //handle for user setting too many bars
            bands = (int)w.ws_col / 2 - 1;

        int height = (int)w.ws_row - 1;
        int width = (int)w.ws_col - bands - 1;

        int bw = width / bands;

#ifndef DEBUG
        int matrix[width][height];
        for (int i = 0; i < width; i++) {
            for (int n = 0; n < height; n++) {
                matrix[i][n] = 0;
            }
        }
#endif

        // process [smoothing]: calculate gravity
        float g = ((float)height / 1000) * powf((60 / (float)framerate), 2.5f);

        //if no bands are selected it tries to padd the default 20 if there is extra room
        if (autoband == 1)
            bands = bands + (((w.ws_col) - (bw * bands + bands - 1)) / (bw + 1));

        width = (int)w.ws_col - bands - 1;

        //checks if there is still extra room, will use this to center
        int rest = (((w.ws_col) - (bw * bands + bands - 1)));
        if (rest < 0)
            rest = 0;

#ifdef DEBUG
        printf("height: %d width: %d bands:%d bandwidth: %d rest: %d\n",
               (int)w.ws_row,
               (int)w.ws_col, bands, bw, rest);
#endif

        float fc[200];
        float fr[200];
        int lcf[200];
        int hcf[200];

        // process: calculate cutoff frequencies
        for (int n = 0; n < bands + 1; n++) {
            // //decided to cut it at 10k, little interesting to hear above
            fc[n] = 10000.0f * powf(10.0f, -2.37f + ((((float)n + 1) / ((float)bands + 1.0f)) * 2.37f));

            // remember nyquist!, pr my calculations this should be rate/2 and
            // nyquist freq in M/2 but testing shows it is not...
            // or maybe the nq freq is in M/4
            fr[n] = fc[n] / (rate / 2);

            // lfc stores the lower cut frequency foo each band in the fft out buffer
            lcf[n] = fr[n] * (M / 4);

            if (n != 0) {
                hcf[n - 1] = lcf[n] - 1;
                if (lcf[n] <= lcf[n - 1])
                    lcf[n] = lcf[n - 1] +
                             1;  //pushing the spectrum up if the expe function gets "clumped"
                hcf[n - 1] = lcf[n] - 1;
            }

#ifdef DEBUG
            if (n != 0)
                printf("%d: %f -> %f (%d -> %d) \n", n, fc[n - 1], fc[n], lcf[n - 1], hcf[n - 1]);
#endif
        }

        float k[200];

        // process: weigh signal to frequencies
        for (int n = 0; n < bands; n++)
            k[n] = powf(fc[n], 0.62f) * (float)height / (M * 2000);

        // output: prepare screen
        int virt = system("setfont cava.psf  >/dev/null 2>&1");
#ifndef DEBUG
        system("setterm -cursor off");
        system("setterm -blank 0");

        // output: reset console
        printf("\033[0m\n");
        system("clear");

        // setting color
        printf("\033[%dm", col);

        //setting "bright" color mode, looks cooler... I think
        printf("\033[1m");
        if (bgcol != 0)
            printf("\033[%dm", bgcol);
        {
            for (int n = (height); n >= 0; n--) {
                for (int i = 0; i < width + bands; i++)
                    //setting backround color
                    printf(" ");
                printf("\n");
            }
            //moving cursor back up
            printf("\033[%dA", height);
        }
#endif

        // general: main loop
        while (true) {
            char ch;
            // general: keyboard controls
            if ((ch = getchar()) != EOF) {
                switch (ch) {
                    case 's':
                        scientificMode = !scientificMode;
                        break;
                    case 'q':
                        cleanup();
                        return EXIT_SUCCESS;
                }
            }

            // output: check if terminal has been resized
            if (virt != 0) {
                ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                if (((int)w.ws_row - 1) != height || ((int)w.ws_col - bands - 1) != width)
                    break;
            }

#ifdef DEBUG
            system("clear");
#endif

            long int lpeak = 0;
            long int hpeak = 0;

            // process: populate input buffer and check if input is present
            for (int i = 0; i < (2 * (M / 2 + 1)); i++) {
                if (i < M) {
                    in[i] = shared[i];
                    if (shared[i] > hpeak)
                        hpeak = shared[i];
                    if (shared[i] < lpeak)
                        lpeak = shared[i];
                } else
                    in[i] = 0;
            }

            int sleep = 0;
            float peak[201];
            peak[bands] = (hpeak + labs(lpeak));

            if (fabs(peak[bands]) == fabs(0.0f))
                sleep++;
            else
                sleep = 0;

            float f[200];

            // process: if input was present for the last 5 seconds apply FFT to it
            if (sleep < framerate * 5) {
                // process: send input to external library
                fftw_execute(p);

                // process: separate frequency bands
                for (int o = 0; o < bands; o++) {
                    peak[o] = 0;

                    // process: get peaks
                    for (int i = lcf[o]; i <= hcf[o]; i++) {
                        // M / 2 + 1 = 1025
                        int y[1025];

                        // getting r of compex
                        y[i] = pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5f);

                        // adding upp band
                        peak[o] += y[i];
                    }

                    // getting average
                    peak[o] = peak[o] / (hcf[o] - lcf[o] + 1);

                    // multiplying with k and adjusting to sens settings
                    float temp = peak[o] * k[o] * ((float)sens / 100.0f);

                    // just in case
                    if (temp > height)
                        temp = height;

                    f[o] = temp;

                    //**falloff function**//
                    if (!scientificMode) {
                        if (temp < flast[o]) {
                            f[o] = fpeak[o] - (g * fall[o] * fall[o]);
                            fall[o]++;
                        }

                        else if (temp >= flast[o]) {
                            f[o] = temp;
                            fpeak[o] = f[o];
                            fall[o] = 0;
                        }

                        // process [smoothing]
                        fmem[o] += f[o];
                        fmem[o] = fmem[o] * 0.55f;
                        f[o] = fmem[o];

                        // memory for falloff func
                        flast[o] = f[o];
                    }

                    if (f[o] < 0.125f)
                        f[o] = 0.125f;

#ifdef DEBUG
                    printf("%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n",
                           o, fc[o], fc[o + 1], lcf[o], hcf[o], peak[o], f[o]);
#endif
                }

            }

            //**if in sleep mode wait and continue**//
            else {
#ifdef DEBUG
                printf("no sound detected for 3 sec, going to sleep mode\n");
#endif
                // wait 1 sec, then check sound again.
                req.tv_sec = 1;
                req.tv_nsec = 0;
                nanosleep(&req, NULL);
                continue;
            }

            if (!scientificMode) {
                // process [smoothing]: monstercat-style "average"
                for (int z = 0; z < bands; z++) {
                    const float smooth[64] = {5, 4.5, 4, 3, 2, 1.5, 1.25, 1.5, 1.5, 1.25, 1.25, 1.5,
                                              1.25, 1.25, 1.5, 2, 2, 1.75, 1.5, 1.5, 1.5, 1.5, 1.5,
                                              1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
                                              1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5,
                                              1.75, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

                    // min val from smooth[]
                    const float sm = 1.25f;
                    const float m_o = 64 / bands;

                    f[z] = f[z] * sm / smooth[(int)floor(z * m_o)];
                    if (f[z] < 0.125f)
                        f[z] = 0.125f;

                    for (int m_y = z - 1; m_y >= 0; m_y--)
                        f[m_y] = fmaxf(f[z] / powf(2, z - m_y), f[m_y]);

                    for (int m_y = z + 1; m_y < bands; m_y++)
                        f[m_y] = fmaxf(f[z] / powf(2, m_y - z), f[m_y]);
                }
            }

// output: draw processed input
#ifndef DEBUG
            switch (om) {
                case 1:
                    for (int n = (height - 1); n >= 0; n--) {
                        int o = 0;

                        //center adjustment
                        int move = rest / 2;
                        for (int i = 0; i < width; i++) {
                            // output: check if we're already at the next bar
                            if (i != 0 && i % bw == 0) {
                                o++;
                                if (o < bands)
                                    move++;
                            }

                            // output: draw and move to another one, check whether we're not too far
                            if (o < bands) {
                                // blank
                                if (f[o] - n < 0.125f) {
                                    if (matrix[i][n] != 0) {
                                        // change?
                                        if (move != 0)
                                            printf("\033[%dC", move);
                                        move = 0;
                                        printf(" ");
                                    }
                                    // no change, moving along
                                    else
                                        move++;
                                    matrix[i][n] = 0;
                                }
                                //color
                                else if (f[o] - n > 1) {
                                    // change?
                                    if (matrix[i][n] != 1) {
                                        if (move != 0)
                                            printf("\033[%dC", move);
                                        move = 0;
                                        printf("\u2588");
                                    }
                                    // no change, moving along
                                    else
                                        move++;
                                    matrix[i][n] = 1;
                                }
                                // top color, finding fraction
                                else {
                                    if (move != 0)
                                        printf("\033[%dC", move);
                                    move = 0;
                                    c = ((((f[o] - (float)n) - 0.125f) / 0.875f * 7) + 1);
                                    if (0 < c && c < 8) {
                                        if (virt == 0)
                                            printf("%d", c);
                                        else
                                            printf("%lc", L'\u2580' + c);
                                    } else
                                        printf(" ");
                                    matrix[i][n] = 2;
                                }
                            }
                        }
                        printf("\n");
                    }
                    printf("\033[%dA", height);
                    break;
            }

            req.tv_sec = 0;
            // sleeping for set us
            req.tv_nsec = (1 / (float)framerate) * 1000000000;
            nanosleep(&req, NULL);
#endif
        }
    }
}
