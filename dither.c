/* Dither images with a 4x4 matrix.
 *
 * See https://github.com/lovell/sharp/issues/455
 */

/* Don't bother with i18n.
 */
#define _(S) (S)

#include <vips/vips.h>

typedef struct _VipsDither {
	VipsOperation parent_instance;

	VipsImage *in;
	VipsImage *out;
} VipsDither;

typedef VipsOperationClass VipsDitherClass;

G_DEFINE_TYPE( VipsDither, vips_dither, VIPS_TYPE_OPERATION );

static int rgb565_matrix[] = {
	1, 9, 3, 11,
	13, 5, 15, 7,
	4, 12, 2, 10,
	16, 8, 14, 6
};

static int
vips_dither_gen( VipsRegion *or, void *vseq, void *a, void *b, gboolean *stop )
{
	VipsRegion *ir = (VipsRegion *) vseq;
	VipsDither *vipsdither = (VipsDither *) b;
	VipsRect *r = &or->valid;

	int x, y;

	if( vips_region_prepare( ir, r ) ) 
		return( -1 );

	for( y = 0; y < r->height; y++ ) { 
		unsigned char *p = (unsigned char *)
			VIPS_REGION_ADDR( ir, r->left, r->top + y );
		unsigned char *q = (unsigned char *)
			VIPS_REGION_ADDR( or, r->left, r->top + y );

		for( x = 0; x < r->width; x++ ) {
			int dx = (x + r->left) & 3;
			int dy = (y + r->top) & 3;
			int dither = rgb565_matrix[(dy << 2) + dx];

			q[0] = VIPS_MIN( p[0] + dither, 255 );
			q[1] = VIPS_MIN( p[1] + dither, 255 );
			q[2] = VIPS_MIN( p[2] + dither, 255 );

			p += 3;
			q += 3;
		}
	}


	return( 0 );
}

static int 
vips_dither_build( VipsObject *object )
{
	const char *nickname = VIPS_OBJECT_GET_CLASS( object )->nickname;
	VipsDither *dither = (VipsDither *) object;

	if( VIPS_OBJECT_CLASS( vips_dither_parent_class )->build( object ) )
		return( -1 );

	/* 8-bit RGB only.
	 */
	if( vips_image_pio_input( dither->in ) || 
		vips_check_uncoded( nickname, dither->in ) ||
		vips_check_format( nickname, dither->in, VIPS_FORMAT_UCHAR ) ||
		vips_check_bands( nickname, dither->in, 3 ) )
		return( -1 );

	g_object_set( dither, "out", vips_image_new(), NULL ); 

	if( vips_image_pipelinev( dither->out, 
		VIPS_DEMAND_STYLE_SMALLTILE, dither->in, NULL ) )
		return( -1 );

	if( vips_image_generate( dither->out,
		vips_start_one, vips_dither_gen, vips_stop_one, 
		dither->in, dither ) )
		return( -1 );

	return( 0 );
}

static void
vips_dither_class_init( VipsDitherClass *klass )
{
	GObjectClass *gobject_class = G_OBJECT_CLASS( klass );
	VipsObjectClass *vobject_class = VIPS_OBJECT_CLASS( klass );
	VipsOperationClass *operation_class = VIPS_OPERATION_CLASS( klass );

	gobject_class->set_property = vips_object_set_property;
	gobject_class->get_property = vips_object_get_property;

	vobject_class->nickname = "dither";
	vobject_class->description = _( "4x4 ordered dither" );
	vobject_class->build = vips_dither_build;

	operation_class->flags = VIPS_OPERATION_SEQUENTIAL;

	VIPS_ARG_IMAGE( klass, "in", 0, 
		_( "Input" ), 
		_( "Input image" ),
		VIPS_ARGUMENT_REQUIRED_INPUT,
		G_STRUCT_OFFSET( VipsDither, in ) );

	VIPS_ARG_IMAGE( klass, "out", 1,
		_( "Output" ), 
		_( "Output image" ),
		VIPS_ARGUMENT_REQUIRED_OUTPUT, 
		G_STRUCT_OFFSET( VipsDither, out ) );

}

static void
vips_dither_init( VipsDither *vipsdither )
{
}

/**
 * vips_dither:
 * @in: input image
 * @out: output image
 *
 * Returns: 0 on success, -1 on failure. 
 */
G_MODULE_EXPORT int
vips_dither( VipsImage *in, VipsImage **out, ... )
{
	va_list ap;
	int result;

	va_start( ap, out );
	result = vips_call_split( "dither", ap, in, out );  
	va_end( ap );

	return( result );
}

/* This is called on module load.
 */
const gchar *
g_module_check_init( GModule *module )
{
#ifdef DEBUG
	printf( "vips_dither: module init\n" ); 
#endif /*DEBUG*/

	vips_dither_get_type();

	/* We can't be unloaded, there would be chaos.
	 */
	g_module_make_resident( module );

	return( NULL ); 
}
