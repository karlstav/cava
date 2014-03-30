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
#include <pthread.h>

struct sigaction old_action;
int M=2048;
int shared[2048];
int debug=0;
int format=-1;
int rate=-1;


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



void*
music(void* data)
{

signed char *buffer;
snd_pcm_t *handle;
snd_pcm_hw_params_t *params;
unsigned int val;
snd_pcm_uframes_t frames;
char *device = ((char*)data);
val=44100;
int i, n, o, size, dir, err,lo;
int tempr,templ;
int radj,ladj;

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

snd_pcm_hw_params_get_format(params, (snd_pcm_format_t * )&val); //getting format	

if(val<6)format=16;
else if(val>5&&val<10)format=24;
else if(val>9)format=32;


snd_pcm_hw_params_get_rate( params, &rate, &dir); //getting rate 	
if(debug==1)printf("detected rate: %d\n",rate);

snd_pcm_hw_params_get_period_size(params,&frames, &dir);   
snd_pcm_hw_params_get_period_time(params,  &val, &dir);

size = frames * (format/8)*2; /* bytes/sample * 2 channels */
buffer = (char *) malloc(size); 
radj=format/4;
ladj=format/8;
o=0;
while(1){

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
        for (i=0; i<size ; i=i+(ladj)*2)
        {
                //              left                            right
                 //structuere [litte],[litte],[big],[big],[litte],[litte],[big],[big]... 
                  
                //first channel           
                tempr = ((buffer[i+(radj)-1 ] << 2));//using the 10 upper bits this whould give me a vert res of 1024, enough...
                lo=((buffer[i+(radj)-2] >> 6));
                if (lo<0)lo=lo+4;
                if(tempr>=0)tempr= tempr + lo;
                if(tempr<0)tempr= tempr - lo;

                //other channel
                templ=(buffer[i+(ladj)-1] << 2);
                lo=(buffer[i+(ladj)-2] >> 6);
                if(lo<0)lo=lo+4;
                if(templ>=0)templ=templ+lo;
                else templ=templ-lo;

                //adding channels and storing it int the buffer
                shared[o]=(tempr+templ)/2;
                o++;
                if(o==M-1)o=0;

                //shifing ringbuffer one to the left, this ended up sing to much cpu..
                //for(o=0;o<M-1;o++) shared[o]=shared[o+1];

                n++;  
                  
        }

    }


}





int main(int argc, char **argv)
{
pthread_t  p_thread; 
int        thr_id; 
char *device = "hw:1,1";
float fc[200];//={150.223,297.972,689.062,1470,3150,5512.5,11025,18000};
float fr[200];//={0.00340905,0.0067567,0.015625,0.0333,0.07142857,0.125,0.25,0.4};
int lcf[200], hcf[200];
float f[200];
int flast[200];
float peak[201];
int y[M/2+1];
long int lpeak,hpeak;
int bands=25;
int sleep=0;
int i, n, o, size, dir, err,bw,width,height,c,rest,virt;
int autoband=1;
//long int peakhist[bands][400];
float temp;
double sum=0;
struct winsize w;
double in[2*(M/2+1)];
fftw_complex out[M/2+1][2];
fftw_plan p;
char *color;
int col = 36;
int sens = 100;
int move=0;
int fall[200];
float fpeak[200];
float k[200];
float g;
int framerate=60;
char *usage="\nUsage : ./cava [options]\n\nOptions:\n\t-b 1..(console columns/2-1) or 200)\t number of bars in the spectrum (default 25 + fills up the console), program wil auto adjust to maxsize if input is to high)\n\n\t-d 'alsa device'\t name of alsa capture device (default 'hw:1,1')\n\n\t-c color\tsuported colors: red, green, yellow, magenta, cyan, white, blue (default: cyan)\n\n\t-s sensitivity %\t sensitivity in percent, 0 means no respons 100 is normal 50 half 200 double and so forth\n\n\t-f framerate \t max frames per second to be drawn, if you are experiencing high CPU usage, try redcing this (default: 60\n\n";
//**END INIT

for (i=0;i<200;i++)
{
flast[i]=0;
fall[i]=0;
fpeak[i]=0;
}
for (i=0;i<M;i++)shared[M]=0;


//**arg handler**//
while ((c = getopt (argc, argv, "b:d:s:f:c:h")) != -1)
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
           case 'f':
             framerate = atoi(optarg);
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
           case 'h':
             printf ("%s",usage);
            return 0; 
           case '?':
             printf ("%s",usage);
             return 1;
           default:
             abort ();
           }


//**ctrl c handler**//

struct sigaction action;
memset(&action, 0, sizeof(action));
action.sa_handler = &sigint_handler;
sigaction(SIGINT, &action, &old_action);





//**getting h*w of term**//
ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
if(bands>(int)w.ws_col/2-1)bands=(int)w.ws_col/2-1; //handle for user setting to many bars
height=(int)w.ws_row-1;
width=(int)w.ws_col-bands-1;
int matrix[width][height];
for(i=0;i<width;i++)
{
    for(n=0;n<height;n++)
    {
    matrix[i][n]=0;
    }
}
bw=width/bands;


g=((float)height/1000)*pow((60/(float)framerate),2.5);//calculating gravity

//if no bands are selected it tries to padd the default 20 if there is extra room
if(autoband==1) bands=bands+(((w.ws_col)-(bw*bands+bands-1))/(bw+1));

//checks if there is stil extra room, will use this to center
rest=(((w.ws_col)-(bw*bands+bands-1)));
if(rest<0)rest=0;


printf("hoyde: %d bredde: %d bands:%d bandbredde: %d rest: %d\n",(int)w.ws_row,(int)w.ws_col,bands,bw,rest);


//**watintg for audio to be ready**//
n=0;
thr_id = pthread_create(&p_thread, NULL, music,(void*)device); //starting music listner
while (format==-1||rate==-1)
    {
    usleep(1000);
    n++;
    if(n>2000)
        {
        printf("could not get rate and or format, problems with audoi thread? quiting...\n");
        exit(1);
        }
    }
printf("got format: %d and rate %d\n",format,rate);

//**calculating cutof frequencies**/
for(n=0;n<bands+1;n++)
{ 
    fc[n]=12000*pow(10,-2.5+(((float)n/(float)bands)*2.5));//decided to cut it at 12k, little interesting to hear above
    fr[n]=fc[n]/(rate); //remember nyquist
    lcf[n]=fr[n]*(M/2+1);

    if(n!=0)
        {
        hcf[n-1]=lcf[n]-1;        
        if(lcf[n]<=lcf[n-1])lcf[n]=lcf[n-1]+1;//pushing the spectrum up if the expe function gets "clumped"
        hcf[n-1]=lcf[n]-1;
        }

    if(debug==1&&n!=0){printf("%d: %f -> %f (%d -> %d) \n",n,fc[n-1],fc[n],lcf[n-1],hcf[n-1]);}
}
//exit(1);

//constants to wigh signal to frequency
for(n=0;n<bands;n++)k[n]=((float)height*log(lcf[n]+2) )/ (1024*(M/4)); 




p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock


//**preparing screen**//
if(debug==0){
virt = system("setfont cava.psf");
system("setterm -cursor off");
//resetting console
printf("\033[0m\n");
system("clear");

printf("\033[%dm",col);//setting volor
printf("\033[1m"); //setting "bright" color mode, looks cooler...

}



//**start main loop**//
while  (1)
{
  if(debug==1){ system("clear");}


//**populating input buffer & checking if there is sound**//
     lpeak=0;
     hpeak=0;
    for (i=0;i<(2*(M/2+1));i++)
    {
       if(i<M)
        {
             in[i]=shared[i];
             if(shared[i]>hpeak) hpeak=shared[i];
             if(shared[i]<lpeak) lpeak=shared[i];
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

        //seperating freq bands
        for(o=0;o<bands;o++)
        {        
            peak[o]=0;

            //getting peaks 
             for (i=lcf[o];i<=hcf[o];i++)
                {
                y[i]=pow(pow(*out[i][0],2)+pow(*out[i][1],2),0.5); //getting r of compex
                if(y[i]>peak[o]) peak[o]=y[i];     
                }

            temp=peak[o]*k[o];  //weighing signal to height and frequency

            
            //**falloff function**//
            if(temp<flast[o])
                {
                f[o]=fpeak[o]-(g*fall[o]*fall[o]);
                fall[o]++;
                }
            else if (temp>=flast[o])
                {
                f[o]=temp;
                fpeak[o]=f[o];                
                fall[o]=0;
                }

            flast[o]=f[o]; //memmory for falloff func
            f[o]=f[o]*((float)sens/100); //adjusting sens

            if(f[o]>height)f[o]=height;//just in case

            if(debug==1){ printf("%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n",o,fc[o],fc[o+1],lcf[o],hcf[o],peak[o],f[o]);}
        }
     // if(debug==1){ printf("topp overall unfiltered:%f \n",peak[bands]); }
 
       
       //if(debug==1){ printf("topp overall alltime:%f \n",sum);}
    }
    else//**if no signal don't bother**//
    {
        if (sleep>(rate*5)/M)//if no signal for 5 sec, go to sleep mode
        {
            if(debug==1)printf("no sound detected for 5 sec, going to sleep mode\n");
            for (i=0;i<200;i++)flast[i]=0; //zeroing memory
            usleep(1*200000);//wait 200 msec, then check sound again. 
            continue;
        }
        if(debug==1)printf("no sound detected, trying again\n");  
        sleep++;
        continue;
    }

//**DRAWING**// -- put in function file maybe?
if (debug==0)
{
    for (n=(height-1);n>=0;n--)
        {
        o=0;
        move=rest/2;//center adjustment  
        //if(rest!=0)printf("\033[%dC",(rest/2));//center adjustment   
        for (i=0;i<width;i++)
            {
            
            //next bar? make a space
            if(i!=0&&i%bw==0){
            o++;
            if(o<bands)move++;
            } 

                       

            //draw color or blank or move+1
           if(o<bands){         //watch so it doesnt draw to far
                if(f[o]-n<0.125) //blank
                            {
                            if(matrix[i][n]!=0)//change?
                                {
                                if(move!=0)printf("\033[%dC",move); 
                                move=0;
                                printf(" ");
                                }
                            else move++; //no change, moving along
                            matrix[i][n]=0;
                            }   
                else if (f[o]-n>1)//color
                            {
                            if(matrix[i][n]!=1)//change?
                                {
                                if(move!=0)printf("\033[%dC",move); 
                                move=0;
                                printf("\u2588");
                                }
                            else move++; //no change, moving along
                            matrix[i][n]=1;
                            }
                else//top color, finding fraction
                    {
                    if(move!=0)printf("\033[%dC",move); 
                    move=0;
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
                    matrix[i][n]=2;
                    }                 
             }   
           
                    
          }
        
        printf("\n");//next line
        
        }

printf("\033[%dA",height);//backup

usleep((1/(float)framerate)*1000000);//sleeping for set us

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
    
