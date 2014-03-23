#include<stdio.h>
#include<stdbool.h>
#include <time.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fftw3.h>
#define PI 3.14159265358979323846
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include <getopt.h>

struct sigaction old_action;


void sigint_handler(int sig_no)
{
    printf("\033[0m\n");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz ");
    system("setterm -cursor on");
    system("clear");        
    printf("CTRL-C pressed -- goodbye\n");
    sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
}


int main(int argc, char **argv)
{
int M=4096;
signed char *buffer;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
unsigned int val;
snd_pcm_uframes_t frames;
char *device = "hw:1,1";
float fc[200];//={150.223,297.972,689.062,1470,3150,5512.5,11025,18000};
float fr[200];//={0.00340905,0.0067567,0.015625,0.0333,0.07142857,0.125,0.25,0.4};
int lcf[200], hcf[200];
double f[200];
double x[M];
double peak[201];
float y[M/2+1];
long int lpeak,hpeak;
int bands=20;
int sleep=0;
float h;
int i, n, o, size, dir, err,xb,yb,bw,format,rate,width,height,c,rest,virt,lo;
int autoband=1;
//long int peakhist[bands][400];
double temp;
double sum=0;
int16_t hi;  
int q=0;
val=44100;
int debug=0;
struct winsize w;
double in[2*(M/2+1)];
fftw_complex out[M/2+1][2];
fftw_plan p;
struct timespec start, stop;
double accum;
char *color;
int col = 37;
int sens = 100;
int beat=0;
//**END INIT






//**arg handler**//
while ((c = getopt (argc, argv, "b:d:s:c:")) != -1)
         switch (c)
           {
           case 'b':
             bands = atoi(optarg);
            autoband=0;  //dont automaticly add bands to fill frame
            if (bands>200)bands=200;
             break;
           case 'd':
             device = optarg;
             break;
           case 's':
             sens = atoi(optarg);
             break;
           case 'c':
            col=0;
             color = optarg;
             if(strcmp(color,"red")==0) col=31;
             if(strcmp(color,"green")==0) col=32;
             if(strcmp(color,"yellow")==0) col=33;
             if(strcmp(color,"blue")==0) col=34;
             if(strcmp(color,"magenta")==0) col=35;
             if(strcmp(color,"cyan")==0) col=36;
             if(strcmp(color,"white")==0) col=37;
             if(col==0)
                {
                printf("color %s not suprted\n",color);
                exit(1);
                }
             break;
           case '?':
             printf ("\nUsage : ./cava [options]\n\nOptions:\n\t-b 1..(console columns/2-1) or 200)\t number of bars in the spectrum (default 20 + fills up the console), program wil auto adjust to maxsize if input is to high)\n\n\t-d 'alsa device'\t name of alsa capture device (default 'hw:1,1')\n\n\t-c color\tsuported colors: red, green, yellow, magenta, cyan, white, blue (default: cyan)\n\n\t-s sensitivity %\t sensitivity in percent, 0 means no respons 100 is normal 50 half 200 double and so forth\n\n");
             return 1;
           default:
             abort ();
           }


//**ctrl c handler**//

struct sigaction action;
memset(&action, 0, sizeof(action));
action.sa_handler = &sigint_handler;
sigaction(SIGINT, &action, &old_action);


//**preparing screen**//
if(debug==0){
virt = system("setfont cava.psf");
system("setterm -cursor off");
}


//getting h*w of term
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
if(bands>(int)w.ws_col/2-1)bands=(int)w.ws_col/2-1; //handle for user setting to many bars
height=(int)w.ws_row-1;
width=(int)w.ws_col-bands-1;
bool matrix[width][height];
bw=width/bands;

//if no bands are selected it tries to padd the default 20 if there is extra room
if(autoband==1) bands=bands+(((w.ws_col)-(bw*bands+bands-1))/(bw+1));

//checks if there is stil extra room, will use this to center
rest=(((w.ws_col)-(bw*bands+bands-1)));
if(rest<0)rest=0;


printf("hoyde: %d bredde: %d bands:%d bandbredde: %d rest: %d\n",(int)w.ws_row,(int)w.ws_col,bands,bw,rest);

//resetting console
printf("\033[0m\n");
system("clear");

printf("\033[%dm",col);//setting volor
printf("\033[1m"); //setting "bright" color mode, looks cooler...





//**init sound device***//

if ((err = snd_pcm_open(&handle, device,  SND_PCM_STREAM_CAPTURE , 0) < 0))
  printf("error opening stream:    %s\n",snd_strerror(err) );
else
 if(debug==1){ printf("open stream succes\n");  }    
snd_pcm_hw_params_alloca(&params);
snd_pcm_hw_params_any (handle, params);
snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE); 
snd_pcm_hw_params_set_channels(handle, params, 2);
val = 44100;
snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
frames = 32;
snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

err = snd_pcm_hw_params(handle, params);
if (err < 0) {
    fprintf(stderr,
            "unable to set hw parameters: %s\n",
            snd_strerror(err));
    exit(1);
}


snd_pcm_hw_params_get_period_size(params,&frames, &dir);   
snd_pcm_hw_params_get_period_time(params,  &val, &dir);

snd_pcm_hw_params_get_format(params, (snd_pcm_format_t * )&val); //getting format	

if(val<6)format=16;
else if(val>5&&val<10)format=24;
else if(val>9)format=32;

size = frames * (format/8)*2; /* bytes/sample * 2 channels */
buffer = (char *) malloc(size); 

if(debug==1)printf("detected format: %d\n",format);

snd_pcm_hw_params_get_rate( params, &rate, &dir); //getting rate 	
if(debug==1)printf("detected rate: %d\n",rate);



//**calculating cutof frequencies**/
for(n=0;n<bands+1;n++)
{ 
    fc[n]=8000*pow(10,-2+(((float)n/(float)bands)*2));//decided to cut it at 10k, little interesting to hear above
    fr[n]=fc[n]/(rate); //remember nyquist
    lcf[n]=fr[n]*(M/2+1);

    if(n!=0)hcf[n-1]=lcf[n];

    if(debug==1&&n!=0){printf("%d: %f -> %f (%d -> %d) \n",n,fc[n-1],fc[n],lcf[n-1],hcf[n-1]);}
}



p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock

//**start main loop**//
while  (1)
{
/*
if( clock_gettime( CLOCK_REALTIME, &start) == -1 ) {
      perror( "clock gettime" );
      exit( EXIT_FAILURE );
}
*/

//**filling of the buffer**//
for(o=0;o<M/32;o++)
{
    err = snd_pcm_readi(handle, buffer, frames);
    if (err == -EPIPE) {
      /* EPIPE means overrun */
       if(debug==1){ fprintf(stderr, "overrun occurred\n");}
      snd_pcm_prepare(handle);
    } else if (err < 0) {
     if(debug==1){ fprintf(stderr, "error from read: %s\n",
              snd_strerror(err));}
    } else if (err != (int)frames) {
     if(debug==1){ fprintf(stderr, "short read, read %d %d frames\n", err,(int)frames);}
   }
   
       //sorting out one channel and only biggest octet
         n=0;//frame counter
        for (i=0; i<size ; i=i+(format/8)*2)
        {
                //              left                            right
                 //structuere [litte],[litte],[big],[big],[litte],[litte],[big],[big]... 
                                
                x[n+(int)frames*o] = ((buffer[i+(format/4)-1 ] << 8)+(buffer[i+(format/8)-1] << 8))/2;//avg of left and right
                if(debug==1)printf("1: %f\n",x[n+(int)frames*o]); 
                lo=((buffer[i+(format/4)-2]+buffer[i+(format/8)-2])/2);
                if (lo<0)lo=lo+255;
                if(x[n+(int)frames*o]>=0)x[n+(int)frames*o]= x[n+(int)frames*o] + lo;
                if(x[n+(int)frames*o]<0)x[n+(int)frames*o]= x[n+(int)frames*o] - lo;
                if(debug==1)printf("2: %d\n",lo);  
                if(debug==1)printf("3: %f\n",x[n+(int)frames*o]); 
                    if(x[n+(int)frames*o]>hpeak) hpeak=x[o];
                    if(x[n+(int)frames*o]<lpeak) lpeak=x[o];
                n++;  
                  
        }
}


  if(debug==1){ system("clear");}


//**populating input buffer & checking if there is sound**//
     lpeak=0;
     hpeak=0;
    for (i=0;i<(2*(M/2+1));i++)
    {
       if(i<M)
        {
             in[i]=x[i];
             if(x[i]>hpeak) hpeak=x[i];
             if(x[i]<lpeak) lpeak=x[i];
        }   
       else in[i]=0;
    // if(debug==1) printf("%d %f\n",i,in[i]);
    }
    peak[bands]=(hpeak+abs(lpeak));

    //**if sound go ahead with fft**//
    if (peak[bands]!=0)    
    {
        sleep=0; //wake if was sleepy
        
        fftw_execute(p);         //applying FFT to signal

       //saving freq domian of input signal
        for (i=0;i<M/2+1;i++)
        {
        y[i]=pow(pow(*out[i][0],2)+pow(*out[i][1],2),0.5);
        }

        //seperating freq bands
        for(o=0;o<bands;o++)
        {        
            peak[o]=0;

            //getting peaks 
             for (i=lcf[o];i<=hcf[o];i++)
                {
                if(y[i]>peak[o]) peak[o]=y[i];     
                }

            //if (peak[o]>sum)sum=peak[o]; peakest values are ussaly around 50k but i have seen 113000 
            //divides on 200 because of complex mplification
            //mulitplise by log of frequency probably because of eq master cd standard, riaa...?


            f[o]=((peak[o]*(float)height*log(lcf[o]+hcf[o]+10)*log(lcf[o]+hcf[o]+10))) /(65536*(M/8)*(log(hcf[bands-1]))) ;  //weighing signal to height and frequency

            f[o]=f[o]*((float)sens/100);

            if(f[o]>height)f[o]=height;//just in case

            if(debug==1){ printf("%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n",o,fc[o],fc[o+1],lcf[o],hcf[o],peak[o],f[o]);}
        }
      if(debug==1){ printf("topp overall unfiltered:%f \n",peak[bands]);
 
        }
       //if(debug==1){ printf("topp overall alltime:%f \n",sum);}
    }
    else//**if no signal don't bother**//
    {
        if (sleep>(rate*5)/M)//if no signal for 5 sec, go to sleep mode
        {
            if(debug==1)printf("no sound detected for 5 sec, going to sleep mode\n");
            usleep(1*1000000);//wait one sec, then check sound again. Maybe break, close and reopen?
            continue;
        }
        if(debug==1)printf("no sound detected, trying again\n");  
        sleep++;
        continue;
    }

//**DRAWING**// -- put in function file maybe?
if (debug==0)
{
/* //**BEAT detection, not yet implemented...
    printf("\033[0m");
    printf("\033[%dm",col);
    if (beat)
        {
        printf("\033[47m");
        beat=0;
        }
*/
    for (n=(height-1);n>=0;n--)
        {
        o=0;   
        for (i=0;i<width;i++)
            {
            
            //next bar? make a space
            if(i!=0&&i%bw==0){
            o++;
            if(o<bands)printf(" ");             
            } 

            //draw color or blank
           if(o<bands){         //watch so it doesnt draw to far
                if(f[o]-n<0.125) printf(" ");   //blank
                else if (f[o]-n>1)  printf("\u2588");//color
                else//top color, finding fraction
                    {
                    c=((((f[o]-(float)n)-0.125)/0.875*7)+1);
                    switch (c)
                           {
                           case 1:
                             if(virt==0)printf("1");
                             else printf("\u2581");
                             break;
                           case 2:
                             if(virt==0)printf("2"); 
                             else printf("\u2582");       
                             break;
                           case 3:
                             if(virt==0)printf("3");
                             else printf("\u2583"); 
                             break;
                           case 4:
                             if(virt==0)printf("4"); 
                             else printf("\u2584");
                             break;
                           case 5:
                             if(virt==0)printf("5");
                             else printf("\u2585"); 
                             break;
                           case 6:
                             if(virt==0)printf("6"); 
                             else printf("\u2586");
                             break;
                           case 7:
                             if(virt==0)printf("7"); 
                             else printf("\u2587");
                             break;
                           default:
                             printf(" "); 
                           }
                    }                  
             }   
           
                    
          }
        printf("\n");//next line
        if(rest!=0)printf("\033[%dC",(rest/2));//center adjustment
        }

printf("\033[%dA",height);//backup


}
//

//snd_pcm_drain(handle);
//snd_pcm_drop(handle);
//snd_pcm_close(handle);
/* //tried to make this frameramet limit to save cpu....
if( clock_gettime( CLOCK_REALTIME, &stop) == -1 ) {
      perror( "clock gettime" );
      exit( EXIT_FAILURE );
}
*/
//accum = (( stop.tv_sec - start.tv_sec )+ ( stop.tv_nsec - start.tv_nsec ))/1000000;
//if(accum<50000&&accum>0)usleep(50000-accum); //16666.666666667 usec = 60 fps  
//printf("time used: %f\n",accum);
}

fftw_destroy_plan(p);
return 0;
}
    
