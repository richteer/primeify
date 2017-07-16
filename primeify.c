#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <pthread.h>
#include "lodepng.h"

#define lodeerror(a) fprintf(stderr,"error %u: %s\n",a,lodepng_error_text(a));

typedef unsigned char image_t;

unsigned int *imagedata;
size_t imagedatasz;
int numthreads = 1;
void (*prep_mode)(mpz_t, unsigned int *, int);
void (*pixel_mode)(mpz_t, unsigned int *, int);
char *source_file;
char *output_file;


int decodePNG(image_t **image, unsigned *width, unsigned *height, char *filename)
{
	return lodepng_decode32_file(image, width, height, filename);
}

int encodePNG(image_t *image, unsigned width, unsigned height, char *filename)
{
	return lodepng_encode32_file(filename, image, width, height);
}

/***** PREP FUNCTIONS *****/
void prep_clear_alpha(mpz_t integ, unsigned int *image, int i)
{
		mpz_set_ui(integ, image[i] & 0x00FFFFFF);
}

/***** CONVERSION FUNCTIONS *****/
void pixel_to_next_prime(mpz_t integ, unsigned int *image, int i)
{
		if (!mpz_probab_prime_p(integ, 15)) {
			mpz_nextprime(integ, integ);
			mpz_export(image + i, NULL, 1, 4, 0, 0, integ);
			if (image[i] & 0xFF000000) {
				image[i] = 0xFFFFFFFF;
			}
			image[i] |= 0xFF000000;
		}
}

void pixel_set_nonprime_black(mpz_t integ, unsigned int *image, int i)
{
	if (!mpz_probab_prime_p(integ, 15)) {
		image[i] = 0xFF000000;
	}
}


int primeify(unsigned int *image, size_t imagesz)
{
	int i,r;
	mpz_t integ;

	mpz_init(integ);
	for (i = 0; i < imagesz; i++) {
		prep_mode(integ, image, i);
		pixel_mode(integ, image, i);
	}
	printf("...done\n");
	mpz_clear(integ);

	return 0;
}


void *prime_thread(void *threadid)
{
	long tid;
	int slice, rem;
	tid = (long) threadid;	

	unsigned int *tmp;
	size_t tmpsz;

	slice = imagedatasz / numthreads;
	rem = imagedatasz % numthreads;

	tmp = imagedata + (slice * tid);

	// Add remainder to the last thread
	primeify(tmp, slice + (rem * !(numthreads - tid - 1)));

	pthread_exit(NULL);
}

void show_help()
{
	printf("Usage: primeify [options] <source PNG> <output PNG>\n");
	printf("\t-p <num>\tNumber of threads to spawn\n");
	printf("\t-b\t\tUse nonprime -> black mode\n");
	printf("\t-n\t\tUse nonprime -> next prime mode\n");
}


void parse_args(int argc, char **argv)
{
	int i;

	// Setting the defaults here I guess
	prep_mode = prep_clear_alpha;
	pixel_mode = pixel_set_nonprime_black;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') break;
		switch (argv[i][1]) {
			case 'b': pixel_mode = pixel_set_nonprime_black; break;
			case 'n': pixel_mode = pixel_to_next_prime; break;

			case 'p': numthreads = atoi(argv[++i]);

		}
	}

	if (argc - i != 2) {
		show_help();
		exit(1);
	}

	source_file = argv[i++];
	output_file = argv[i];

}

int main(int argc, char **argv)
{
	pthread_t *threads;
	image_t *image;
	unsigned width, height;
	int ret;
	long t;

	parse_args(argc, argv);

	ret = decodePNG(&image, &width, &height, source_file);
	if (ret) {
		lodeerror(ret);
		return ret;
	}

	imagedata = (unsigned int *) image;
	imagedatasz = width*height;

	threads = malloc(sizeof(pthread_t) * numthreads);

	for (t = 0; t < numthreads; t++) {
		pthread_create(threads + t, NULL, prime_thread, (void *) t);
	}

	for (t = 0; t < numthreads; t++) {
		pthread_join(threads[t], NULL);
	}

	ret = encodePNG(image, width, height, output_file);
	if (ret) {
		lodeerror(ret);
		return ret;
	}

	free(image);	

}
