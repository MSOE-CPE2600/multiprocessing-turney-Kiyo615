/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//   Revised by: Carson Schur
// 	 CPE 2600 
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "jpegrw.h"
#include <sys/wait.h>
#include <sys/mman.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <bits/getopt_core.h>
#include <errno.h>

#define MAX_THREADS 20

typedef struct {
		int start;  
		int end;    
	} Section;

typedef struct {
		imgRawImage* img;
		pthread_t* thread;
		int nthreads;
		double xmin;
		double xmax;
		double ymin;
		double ymax;
		int max;
		Section* sections;
	}arguments;

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image(imgRawImage* img, pthread_t *thread, int nthreads, double xmin, double xmax,  
									double ymin, double ymax, int max);
void *compute_image_thread(void *ptr);
static void show_help();


int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	char outfile[256] = "mandel";
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int active_proc = 0;
	int procs = 2;
	int frames = 50;
	int nthreads = 2;

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"n:f:t:x:y:s:W:H:m:o:h"))!=-1) {
		switch(c){
			case 'n':
				procs = atoi(optarg);
				break;
			case 'f':
				frames = atoi(optarg);
				break;
			case 't':
				nthreads = atoi(optarg);
				break;
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				strcpy(outfile, optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	pthread_t thread[nthreads];

	for(int i = 0; i < frames; i++) {
		// create child processes for 
    	if (active_proc >= procs){
            wait(NULL);
            active_proc--;
        }
        int pid = fork();
		//printf("%i\n",pid);
        if (pid == 0){
			// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
			xscale = xscale /pow(1.2,i);
			yscale = xscale / (image_width ) * (image_height );

			// change file name per frame
			char file_name[512];
			snprintf(file_name, sizeof(file_name), "%s%i.jpg", outfile, i);

			// Display the configuration of the image.
			printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,file_name);

			// Create a raw image of the appropriate size.
			imgRawImage* img = initRawImage(image_width,image_height);

			// Fill it with a black
			setImageCOLOR(img,0);

			// Compute the Mandelbrot image
			compute_image(img,thread,nthreads,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/2,ycenter+yscale/2,max);

			// Save the image in the stated file.
			storeJpegImageFile(img,file_name);

			// free the mallocs
			freeRawImage(img);
		
            exit(0);
        }else if (pid > 0) { //This is the parent
            active_proc++;
        }
	}
	/*while (active_proc > 0){
        wait(NULL);
        active_proc--;
    }*/

	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}


/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/
void compute_image(imgRawImage* img, pthread_t *thread, int nthreads,
                   double xmin, double xmax, double ymin, double ymax, int max){
    int height = img->height;

    Section sections[MAX_THREADS];
    arguments args[MAX_THREADS];

    int baseRows = height / nthreads;
    int leftover = height % nthreads;
    int posRow = 0;

    for (int k = 0; k < nthreads; k++) {
        int rowsHere = baseRows + (k < leftover ? 1 : 0);

        sections[k].start = posRow;
        sections[k].end   = posRow + rowsHere;
        posRow += rowsHere;

        args[k].img = img;
        args[k].xmin = xmin; args[k].xmax = xmax;
        args[k].ymin = ymin; args[k].ymax = ymax;
        args[k].max = max;
        args[k].sections = &sections[k];

        int rc = pthread_create(&thread[k], NULL, compute_image_thread, &args[k]);
        if (rc != 0) { errno = rc; perror("pthread_create"); }
    }

    for (int k = 0; k < nthreads; k++) {
        int rc = pthread_join(thread[k], NULL);
        if (rc != 0) { errno = rc; perror("pthread_join"); }
    }
}

void *compute_image_thread(void *ptr){
    arguments *args = (arguments *)ptr;

    int width  = args->img->width;
    int height = args->img->height;

    int start = args->sections->start;
    int end   = args->sections->end;

    for (int i = start; i < end; i++) {
        for (int j = 0; j < width; j++) {

            double x = args->xmin + i * (args->xmax - args->xmin) / width;
            double y = args->ymin + j * (args->ymax - args->ymin) / height;

            int iters = iterations_at_point(x, y, args->max);
            setPixelCOLOR(args->img, i, j, iteration_to_color(iters, args->max));
        }
    }
    return NULL;
}

/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
