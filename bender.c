/*
 * vim: ts=4:sw=4:fdm=marker
 *
 * (c) 2014 Przemyslaw Iskra <sparky@pld-linux.org>
 * You can use this code under the terms of AGPL license.
 *
 * compile with:
 *
 * gcc -std=c99 -O2 -Wall -lpng -lm bender.c -o bender
 */


#include <stdio.h>
#include <math.h>
#include <stdarg.h> /* va_* */
#include <stdlib.h> /* abort() */
#include <stdbool.h> /* c99 boolean */
#include <string.h> /* strlen */

#define PNG_DEBUG 3
#include <png.h>

/* always 8-bit RGBA */
#define BYTES_PER_PIXEL 4
#define BITS_PER_CHANNEL 8

typedef struct coord_s
{
	double x;
	double y;
} coord_t;

typedef struct coord_int_s
{
	long unsigned int x;
	long unsigned int y;
} coord_int_t;

typedef struct image_file_s
{
	png_bytepp row_pointers;
	long unsigned int width;
	long unsigned int height;
} image_file_t;

typedef struct pixel_rgba_s
{
	png_byte r;
	png_byte g;
	png_byte b;
	png_byte a;
} pixel_rgba_t;

typedef struct bokeh_circle_s
{
	unsigned long int outx;
	unsigned long int outy;
	unsigned long int width;
	unsigned long int height;
	double pixel[0];
} bokeh_circle_t;

typedef struct pixel_partial_s
{
	double r;
	double g;
	double b;
	double a;
} pixel_partial_t;

typedef struct transform_table_s
{
	unsigned long int output_width;
	unsigned long int output_height;
	unsigned long int patch_width;
	unsigned long int patch_height;
	double alpha_fix;
	bokeh_circle_t **row_pointers[0];
} transform_table_t;

typedef struct args_s
{
	coord_int_t bg_size, patch_size;
	coord_t point_left_top;
	coord_t point_left_bottom;
	coord_t point_right_top;
	coord_t point_right_bottom;
	coord_t point_middle_top;
	coord_t point_middle_bottom;
	double angle_start;
	double angle_stop;
	coord_t bokeh_f1;
	coord_t bokeh_f2;
	double bokeh_r1;
	double bokeh_r2;
} args_t;

#define POINT_BG_SIZE		0
#define POINT_PATCH_SIZE	1
#define POINT_LEFT_TOP		2
#define POINT_LEFT_BOTTOM	3
#define POINT_RIGHT_TOP		4
#define POINT_RIGHT_BOTTOM	5
#define POINT_MIDDLE_TOP	6
#define POINT_MIDDLE_BOTTOM 7
#define POINT_ANGLES		8
#define POINT_FOCUS_F1		9
#define POINT_FOCUS_F2		10
#define POINT_FOCUS_R		11

#define die( args... ) die_( args ) /* {{{ */
void die_( const char *s, ... )
{
	va_list args;
	va_start( args, s );
	vfprintf( stderr, s, args );
	fprintf( stderr, "\n" );
	va_end( args );
	abort();
} /* }}} */

image_file_t *image_from_file( const char *filename ) /* {{{ */
{
	unsigned char header[8];
	image_file_t *image;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytepp row_pointers;
	int tmp, y;

	image = malloc( sizeof( image_file_t ) );

	FILE *fp = fopen( filename, "rb" );
	if ( !fp )
		die( "Cannot open file '%s'", filename );

	fread( header, 1, 8, fp );
	if ( png_sig_cmp( header, 0, 8 ) )
		die( "File '%s' is not a png", filename );

	png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( ! png_ptr )
		die( "png_create_read_struct failed" );

	info_ptr = png_create_info_struct( png_ptr );
	if ( ! info_ptr )
		die( "png_create_info_struct failed" );

	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		/* XXX: handle errors here */
		die( "Error during image initialization" );
	}

	png_init_io( png_ptr, fp );
	png_set_sig_bytes( png_ptr, 8 );
	png_read_info( png_ptr, info_ptr );
	png_set_add_alpha( png_ptr, 255, PNG_FILLER_AFTER );
	png_set_gray_to_rgb( png_ptr );
	png_set_palette_to_rgb( png_ptr );
	png_set_expand( png_ptr );
	png_set_scale_16( png_ptr );

	image->width = png_get_image_width( png_ptr, info_ptr );
	image->height = png_get_image_height( png_ptr, info_ptr );

	tmp = png_set_interlace_handling( png_ptr );

	png_read_update_info( png_ptr, info_ptr );

	tmp = png_get_color_type( png_ptr, info_ptr );
	if ( tmp != PNG_COLOR_TYPE_RGBA )
		die( "Color type is %d, but it should be %d", tmp, PNG_COLOR_TYPE_RGBA );

	tmp = png_get_bit_depth( png_ptr, info_ptr );
	if ( tmp != BITS_PER_CHANNEL )
		die( "Color depth is %d, but is should be %d", tmp, BITS_PER_CHANNEL );
	
	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		/* XXX: handle errors here */
		die( "Error during image read" );
	}

	image->row_pointers = row_pointers =
		malloc( sizeof(png_bytep) * image->height );
	if ( !row_pointers )
		die( "Cannot allocate pointer memory" );

	for ( y = 0; y < image->height; y++ )
	{
		row_pointers[ y ] = malloc( image->width * BYTES_PER_PIXEL );
		if ( !row_pointers[ y ] )
			die( "Cannot allocate pointer memory for row %d", y );
	}

	png_read_image( png_ptr, row_pointers );
	png_read_end( png_ptr, info_ptr );

	fclose( fp );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );

	return image;
} /* }}} */

image_file_t *image_new( long unsigned int width, long unsigned int height ) /* {{{ */
{
	long unsigned int y;
	png_bytepp row_pointers;
	image_file_t *image;

	image = malloc( sizeof( image_file_t ) );
	image->width = width;
	image->height = height;

	image->row_pointers = row_pointers =
		malloc( sizeof(png_bytep) * image->height );
	if ( !row_pointers )
		die( "Cannot allocate pointer memory" );

	for ( y = 0; y < image->height; y++ )
	{
		row_pointers[ y ] = calloc( BYTES_PER_PIXEL, image->width );
		if ( !row_pointers[ y ] )
			die( "Cannot allocate pointer memory for row %d", y );
	}

	return image;
} /* }}} */

void image_write( image_file_t * restrict image, const char * restrict filename ) /* {{{ */
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;

	fp = fopen( filename, "wb" );
	if ( !fp )
	{
		die( "Could not open file %s for writing", filename );
	}

	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( ! png_ptr )
		die( "png_create_write_struct failed" );

	info_ptr = png_create_info_struct( png_ptr );
	if ( ! info_ptr )
		die( "png_create_info_struct failed" );

	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		/* XXX: handle errors here */
		die( "Error during png writing" );
	}

	png_init_io( png_ptr, fp );
	png_set_IHDR( png_ptr, info_ptr, image->width, image->height,
			BITS_PER_CHANNEL, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

	png_write_info( png_ptr, info_ptr );

	png_write_image( png_ptr, image->row_pointers );

	png_write_end( png_ptr, NULL );

	png_destroy_write_struct( &png_ptr, &info_ptr );

	fclose( fp );
} /* }}} */

void image_destroy( image_file_t **image ) /* {{{ */
{
	long unsigned int y;
	png_bytepp row_pointers = (*image)->row_pointers;

	for ( y = 0; y < (*image)->height; y++ )
		free( row_pointers[ y ] );
	free( row_pointers );

	free( *image );
	*image = NULL;
} /* }}} */

static double
coord_dist( const coord_t * restrict a, const coord_t * restrict b )
{
	double x = b->x - a->x;
	double y = b->y - a->y;
	return sqrt( x * x + y * y );
}

static inline coord_t
coord_subst( const coord_t * restrict a, const coord_t * restrict b )
{
	coord_t output;
	output.x = a->x - b->x;
	output.y = a->y - b->y;
	return output;
}

static inline coord_t
coord_add( const coord_t * restrict a, const coord_t * restrict b )
{
	coord_t output;
	output.x = a->x + b->x;
	output.y = a->y + b->y;
	return output;
}

static inline coord_t
coord_scale( const coord_t * restrict a, double mult )
{
	coord_t output;
	output.x = a->x * mult;
	output.y = a->y * mult;
	return output;
}

static inline coord_t
coord_middle( const coord_t * restrict a, const coord_t * restrict b )
{
	coord_t tmp = coord_add( a, b );
	return coord_scale( &tmp, 0.5 );
}

/* calculate intersection of two lines, each one defined by 2 points */
static coord_t
calc_intersection(
		const coord_t * restrict a,
		const coord_t * restrict b,
		const coord_t * restrict c,
		const coord_t * restrict d ) /* {{{ */
{
	coord_t v1, v2;
	double down, k;

	v1 = coord_subst( a, b );
	v2 = coord_subst( c, d );

	down = v1.x * v2.y - v1.y * v2.x;
	if ( ! down )
		die( "Lines are parallel" );

	k = v2.y * ( c->x - a->x ) + v2.x * ( a->y - c->y );

	v2 = coord_scale( &v1, k / down );
	return coord_add( a, &v2 );
} /* }}} */

static bokeh_circle_t *
calc_bokeh_circle( const coord_t *out, double r ) /* {{{ */
{
	bokeh_circle_t *circle;
	unsigned long int x, y, width, height;
	double dy1, dy2, dx1, dx2, tmp, cx, cy, sum = 0;
	if ( r < 0.75 )
		r = 0.75;
	double r2 = r * r;
	double r_int = ceil( r - 0.5 );

	cx = r_int - 1 + out->x - floor( out->x );
	cy = r_int - 1 + out->y - floor( out->y );

	if ( cx < r - 0.5 )
		cx += 1;
	if ( cy < r - 0.5 )
		cy += 1;
	width = ceil( cx + r + 0.5 );
	height = ceil( cy + r + 0.5 );

	circle = malloc( sizeof( bokeh_circle_t ) + sizeof( double ) * width * height );
	circle->width = width;
	circle->height = height;
	/* sould be integers, but it is safer to round */
	circle->outx = round( out->x - cx );
	circle->outy = round( out->y - cy );

	for ( y = 0; y < height; y++ )
	{
		dy1 = y - 0.5 - cy; dy1 *= dy1;
		dy2 = y + 0.5 - cy; dy2 *= dy2;
		if ( dy2 > dy1 )
		{
			tmp = dy1;
			dy1 = dy2;
			dy2 = tmp;
		}

		for ( x = 0; x < width; x++ )
		{
			double value = 0;
			dx1 = x - 0.5 - cx; dx1 *= dx1;
			dx2 = x + 0.5 - cx; dx2 *= dx2;
			if ( dx2 > dx1 )
			{
				tmp = dx1;
				dx1 = dx2;
				dx2 = tmp;
			}

			if ( dx1 + dy1 < r2 )
			{
				value = 1;
			}
			else if ( dx2 + dy2 < r2 )
			{
				double d_max, d_min, diff, filled;
				d_max = sqrt( dx1 + dy1 );
				d_min = sqrt( dx2 + dy2 );
				diff = d_max - d_min;
				filled = r - d_min;
				value = filled / diff;
			}

			circle->pixel[ y * width + x ] = value;
			sum += value;
		}
	}

	/* normalize */
	for ( y = 0; y < height; y++ )
	{
		for ( x = 0; x < width; x++ )
		{
			circle->pixel[ y * width + x ] /= sum;
		}
	}

	return circle;
} /* }}} */

static coord_t *
calc_hiperbolic_distribution(
		const coord_t * restrict origin,
		const coord_t * restrict a,
		const coord_t * restrict b,
		unsigned long int divisions ) /* {{{ */
{
	unsigned long int i;
	double d1, d2, h, m;
	coord_t v1, v2, *out;

	d1 = coord_dist( origin, a );
	d2 = coord_dist( origin, b );

	h = (double) divisions * d2 / ( d2 - d1 );
	m = - h * d1;

	v1 = coord_subst( a, origin );
	/*v1.x = a->x - origin->x;
	v1.y = a->y - origin->y;*/

	out = malloc( sizeof( coord_t ) * divisions );

	for ( i = 0; i < divisions; i++ )
	{
		double y = m / ( (double) i - h );
		v2 = coord_scale( &v1, y / d1 );
		/*v2.x = v1.x * y / d1;
		v2.y = v1.y * y / d1;*/
		out[ i ] = coord_add( origin, &v2 );
		/*out[ i ].x = origin->x + v2.x;
		out[ i ].y = origin->y + v2.y;*/
	}

	return out;
} /* }}} */

static void
calc_half_ellipse(
		const coord_t * const restrict center,
		const coord_t * const restrict middle,
		const coord_t * const restrict side,
		double angle_start, double angle_stop, unsigned long int divisions,
		coord_t * restrict output ) /* {{{ */
{
	double r1, r2, dist_middle, angle_r1, beta_sin, beta_cos, angle_increment;
	unsigned long int i;

	r1 = coord_dist( center, side );
	dist_middle = coord_dist( center, middle );
	angle_r1 = atan2( side->y - center->y, side->x - center->x );

	beta_sin = sin( angle_r1 );
	beta_cos = cos( angle_r1 );

	{
		double angle_middle = atan2( middle->y - center->y, middle->x - center->x );
		double alpha = - angle_r1 + angle_middle;
		double alpha_sin, alpha_cos, under;
		alpha_sin = sin( alpha );
		alpha_cos = cos( alpha );
		under = r1 * r1 - ( dist_middle * alpha_cos ) * ( dist_middle * alpha_cos );
		if ( under <= 0 )
			die( "Wrong distance in half_ellipse" );
		r2 = ( dist_middle * r1 * alpha_sin ) / sqrt( under );
	}

	angle_increment = ( angle_stop - angle_start ) / ( divisions - 1 );
	for ( i = 0; i < divisions; i++ )
	{
		double alpha = angle_start + i * angle_increment;
		double alpha_sin, alpha_cos;
		alpha_sin = sin( alpha );
		alpha_cos = cos( alpha );
		output[ i ].x = center->x + r1 * alpha_cos * beta_cos - r2 * alpha_sin * beta_sin;
		output[ i ].y = center->y + r1 * alpha_cos * beta_sin + r2 * alpha_sin * beta_cos;
	}
} /* }}} */

#define BOKEH_SHARP 2.5
#define BOKEH_BLURRY 5
static bokeh_circle_t **
calc_transform_line_bokeh(
		const coord_t * restrict points, unsigned long int count,
		const coord_t * restrict e_f1,
		const coord_t * restrict e_f2,
		double r1, double r2 ) /* {{{ */
{
	double d, bokeh_inc;
	unsigned long int i;
	bokeh_circle_t **output;

	output = malloc( sizeof( bokeh_circle_t * ) * count );

	/* at distance r1 bokeh is BOKEH_SHARP
	 * at distance r2 bokeh is BOKEH_BLURRY
	 */
	bokeh_inc = ( BOKEH_BLURRY - BOKEH_SHARP ) / ( r2 - r1 );
	for ( i = 0; i < count; i++ )
	{
		const coord_t *p = points + i;
		double ball_r = BOKEH_SHARP;
		d = coord_dist( e_f1, p ) + coord_dist( e_f2, p );
		if ( d < r1 )
		{
			ball_r = BOKEH_SHARP;
		}
		else
		{
			ball_r = BOKEH_SHARP + ( d - r1 ) * bokeh_inc;
		}

		output[ i ] = calc_bokeh_circle( p, ball_r );
	}

	return output;
} /* }}} */

#ifdef __GNUC__
static bokeh_circle_t **
calc_transform_line_sharp( const coord_t *points, unsigned long int count )
	__attribute__ ((unused));
#endif
static bokeh_circle_t **
calc_transform_line_sharp( const coord_t *points, unsigned long int count ) /* {{{ */
{
	unsigned long int i;
	bokeh_circle_t **output;

	output = malloc( sizeof( bokeh_circle_t * ) * count );
	for ( i = 0; i < count; i++ )
	{
		const coord_t *p = points + i;
		output[ i ] = calc_bokeh_circle( p, BOKEH_SHARP );
	}

	return output;
} /* }}} */

static transform_table_t *
calc_transform_table( coord_t *list, long int points ) /* {{{ */
{
	coord_t m1, m2, end;
	coord_t *h_c, *h_t, *h_s;
	coord_t *ellipse_tmp;
	transform_table_t *output;
	unsigned long int input_width, input_height, output_width, output_height;
	double angle_start, angle_stop;
	double bokeh_r1, bokeh_r2;
	int i;

	input_width = list[ POINT_PATCH_SIZE ].x;
	input_height = list[ POINT_PATCH_SIZE ].y;
	output_width = list[ POINT_BG_SIZE ].x;
	output_height = list[ POINT_BG_SIZE ].y;

	m1 = coord_middle( list + POINT_LEFT_TOP, list + POINT_RIGHT_TOP );
	m2 = coord_middle( list + POINT_LEFT_BOTTOM, list + POINT_RIGHT_BOTTOM );

	end = calc_intersection( list + POINT_LEFT_TOP, list + POINT_LEFT_BOTTOM,
			list + POINT_RIGHT_TOP, list + POINT_RIGHT_BOTTOM );

	h_c = calc_hiperbolic_distribution( &end, &m1, &m2, input_height );

	h_t = calc_hiperbolic_distribution( &end,
			list + POINT_MIDDLE_TOP, list + POINT_MIDDLE_BOTTOM,
			input_height );
	h_s = calc_hiperbolic_distribution( &end,
			list + POINT_LEFT_TOP, list + POINT_LEFT_BOTTOM,
			input_height );

	angle_start = list[ POINT_ANGLES ].x;
	angle_stop = list[ POINT_ANGLES ].y;
	bokeh_r1 = list[ POINT_FOCUS_R ].x;
	bokeh_r2 = list[ POINT_FOCUS_R ].y;

	ellipse_tmp = malloc( sizeof( coord_t ) * input_width );
	output = malloc( sizeof( transform_table_t )
			+ sizeof( bokeh_circle_t ** ) * input_height );

	output->patch_width = input_width;
	output->patch_height = input_height;
	output->output_width = output_width;
	output->output_height = output_height;

	{
		double x1, x2;
		x1 = list[ POINT_RIGHT_TOP ].x - list[ POINT_LEFT_TOP ].x;
		x2 = list[ POINT_RIGHT_BOTTOM ].x - list[ POINT_LEFT_BOTTOM ].x;
		if ( x2 > x1 )
			x1 = x2;
		if ( x1 * 1.5 > input_width )
		{
			output->alpha_fix = x1 * 1.5 / input_width;
		}
		else
		{
			output->alpha_fix = 1;
		}
	}

	for ( i = 0; i < input_height; i++ )
	{
		calc_half_ellipse(
			h_c + i, h_t + i, h_s + i,
			angle_start, angle_stop,
			input_width,
			ellipse_tmp
		);

		/* output->row_pointers[ i ] = calc_transform_line_sharp( ellipse_tmp, input_width ); */
		output->row_pointers[ i ] = calc_transform_line_bokeh(
				ellipse_tmp, input_width,
				list + POINT_FOCUS_F1, list + POINT_FOCUS_F2,
				bokeh_r1, bokeh_r2
				);
	}
	free( ellipse_tmp );
	free( h_c );
	free( h_t );
	free( h_s );

	return output;
} /* }}} */

static void
destroy_transform_table( transform_table_t **tt )
{
	int i, j;

	for ( i = 0; i < (*tt)->patch_height; i++ )
	{
		bokeh_circle_t **row = (*tt)->row_pointers[ i ];
		for ( j = 0; j < (*tt)->patch_width; j++ )
		{
			free( row[ j ] );
		}
		free( row );
	}
	free( *tt );
	*tt = NULL;
}

static void
image_process( transform_table_t *transform_table,
		const char *in_file, const char *out_file,
		long int move_x, long int move_y ) /* {{{ */
{
	printf( "Image process %s -> %s with +%ld+%ld\n",
			in_file, out_file,
			move_x, move_y
		);

	image_file_t *img_in;
	img_in = image_from_file( in_file );

	long int do_width, do_height;

	do_width = transform_table->patch_width;
	if ( img_in->width < do_width )
		do_width = img_in->width;
	do_height = transform_table->patch_height;
	if ( img_in->height < do_height )
		do_height = img_in->height;

	int x, y, bx, by;
	pixel_partial_t *ppix;
	ppix = calloc( sizeof( pixel_partial_t ), transform_table->output_width * transform_table->output_height );

	for ( y = move_y; y < do_height; y++ )
	{
		bokeh_circle_t **bc_row;
		bc_row = transform_table->row_pointers[ y ];
		pixel_rgba_t *in_row = (pixel_rgba_t *) img_in->row_pointers[ y ];

		for ( x = move_x; x < do_width; x++ )
		{
			bokeh_circle_t *bokeh = bc_row[ x ];
			pixel_rgba_t *p_in;
			p_in = in_row + x;
			
			if ( bokeh->outx >= transform_table->output_width )
			{
				printf( "Pixel [%dx%d] out of horizontal bounds, max: %ld, found %ld\n",
					x, y, transform_table->output_width, bokeh->outx );
				continue;
			}
			if ( bokeh->outy >= transform_table->output_height )
			{
				printf( "Pixel [%dx%d] out of horizontal bounds, max: %ld, found %ld\n",
					x, y, transform_table->output_height, bokeh->outy );
				continue;
			}

			//printf( "bokeh size: %d %d\n", bokeh->height, bokeh->width );
			for ( by = 0; by < bokeh->height; by++ )
			{
				for ( bx = 0; bx < bokeh->width; bx++ )
				{
					double bokeh_alpha = bokeh->pixel[ by * bokeh->width + bx ];
					pixel_partial_t *p_out;
					p_out = ppix
						+ ( bokeh->outy + by ) * transform_table->output_width
						+ ( bokeh->outx + bx );

					bokeh_alpha *= ( double ) p_in->a / 255.0;
					bokeh_alpha *= transform_table->alpha_fix;

					p_out->r += bokeh_alpha * p_in->r;
					p_out->g += bokeh_alpha * p_in->g;
					p_out->b += bokeh_alpha * p_in->b;
					p_out->a += bokeh_alpha;
				}
			}
		}
	}

	image_file_t *img_out;
	img_out = image_new( transform_table->output_width, transform_table->output_height );

	for ( y = 0; y < transform_table->output_height; y++ )
	{
		pixel_rgba_t *row = (pixel_rgba_t *) img_out->row_pointers[ y ];
		for ( x = 0; x < transform_table->output_width; x++ )
		{
			pixel_partial_t *p_in = ppix + y * transform_table->output_width + x;
			pixel_rgba_t *p_out = row + x;
			if ( ! p_in->a )
				continue;

			double fix = 1 / p_in->a;
			if ( p_in->a > 1 )
			{
				p_out->a = 255;
			}
			else
			{
				p_out->a = 255 * p_in->a;
			}

			p_out->r = p_in->r * fix;
			p_out->g = p_in->g * fix;
			p_out->b = p_in->b * fix;
		}
	}

	image_write( img_out, out_file );
	image_destroy( &img_out );
	image_destroy( &img_in );
	free( ppix );
} /* }}} */

int
main( int argc, char **argv )
{
	int i, count = 24;

	if ( argc - 1 < 26 )
	{
		printf( "%s requires at least 26 arguments. You should try not run it manually.\n",
				argv[0]
			  );
		exit(0);
	}

	double tmp = 0;
	coord_t data[ 12 ];

	for ( i = 0; i < 24; i++ )
	{
		char *arg = argv[ i + 1 ];
		char *tail = NULL;
		char *end = arg + strlen( arg );
		double out;
		out = strtod( arg, &tail );
		// printf( "- %p, %p, %f\n", argv[i], tail, out );
		//
		if ( tail != end )
		{
			die( "Invalid number '%s' in argument %d", arg, i );
		}
		//printf( "number: %f <- %s\n", out, arg );
		if ( i % 2 )
		{
			data[ i / 2 ].x = tmp;
			data[ i / 2 ].y = out;
		}
		else
		{
			tmp = out;
		}
	}

	transform_table_t *transform_table;
	transform_table = calc_transform_table( data, count / 2 );

	char *in_file = NULL, *arg;
	long int move_x = 0, move_y = 0;

	i = 24;
	while ( ++i < argc )
	{
		arg = argv[ i ];
		if ( arg[0] == '+' || arg[0] == '-' )
		{
			char *tail = NULL, *tail2 = NULL;
			char *end = arg + strlen( arg );
			move_x = strtol( arg, &tail, 10 );
			if ( !tail || tail == arg )
				die( "Invalid position value in '%s'\n", arg );
			move_y = strtol( tail, &tail2, 10 );
			if ( !tail2 || tail2 == tail || tail2 != end )
				die( "Invalid position value in '%s'\n", arg );
		}
		else if ( ! in_file )
		{
			in_file = arg;
		}
		else
		{
			image_process( transform_table, in_file, arg, move_x, move_y );
			in_file = NULL;
			move_x = 0;
			move_y = 0;
		}
	}

	if ( in_file )
	{
		printf( "Warning, there are unprocessed arguments: '%s'\n", in_file );
	}

	destroy_transform_table( &transform_table );

	return 0;
}

/*
	image_file_t *image;
	coord_t center = { 20.25, 22.25 };
	bokeh_circle_t *circle = calc_bokeh_circle( &center, 0.75 );
	image = image_new( circle->width, circle->height );

	{
		unsigned long int x, y;
		for ( y = 0; y < circle->height; y++ )
		{
			pixel_rgba_t *row = (pixel_rgba_t *) image->row_pointers[ y ];
			for ( x = 0; x < circle->width; x++ )
			{
				pixel_rgba_t *pixel = row + x;
				double value = circle->pixel[ y * circle->width + x ];
				double value2 = 0;
				value *= circle->width * circle->height * 255;
				//printf( "value: %f\n", value );
				if ( value > 255 )
				{
					value2 = value / 4;
					value = 255;
				}
				pixel->r = value;
				pixel->g = value2;
				pixel->b = 0;
				pixel->a = 255;
			}
		}
	}
	image_write( image, "out/test.png" );
	free( circle );
	image_destroy( &image );
	return 0;
}*/
