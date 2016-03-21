#ifndef _SHAPE_INFO_H_
#define _SHAPE_INFO_H_

#include "item_type.h"
#include "shape_bits.h"

typedef struct dimension_set {
	dimension_t	ds_dimension[N_DIMENSIONS];
	uint32_t	ds_n_elts;	/* total number of elements */
} Dimension_Set;

#define DS_N_ELTS(dsp)			(dsp)->ds_n_elts
#define DS_DIM(dsp,idx)			(dsp)->ds_dimension[idx]
#define SET_DS_N_ELTS(dsp,v)		(dsp)->ds_n_elts = v
#define SET_DS_DIM(dsp,idx,v)		(dsp)->ds_dimension[idx] = v


#define STRING_DIMENSION(dsp,len)		\
{						\
	dsp->ds_dimension[0]=1;			\
	dsp->ds_dimension[1]=len;		\
	dsp->ds_dimension[2]=1;			\
	dsp->ds_dimension[3]=1;			\
	dsp->ds_dimension[4]=1;			\
}


/* We have a problem with complex objects - in order to be able to
 * treat the real and complex parts as components, the dimensions
 * and increments reflect the machine precision of the elements.
 * But in vector operations we use a complex pointer, so they
 * are too much...  We could have the pointer be to the basic machine
 * type instead???
 */

typedef struct increment_set {
	incr_t		is_increment[N_DIMENSIONS];
} Increment_Set;


typedef struct precision {
	Item			prec_item;
	prec_t			prec_code;
	int			prec_size;	// type size in bytes
	struct precision *	prec_mach_p;	// NULL for machine precisions
} Precision;

#define NO_PRECISION	((Precision *)NULL)
#define prec_name	prec_item.item_name


#define PREC_NAME(prec_p)		(prec_p)->prec_item.item_name

#define PREC_CODE(prec_p)		(prec_p)->prec_code
#define PREC_SIZE(prec_p)		(prec_p)->prec_size
#define PREC_MACH_PREC_PTR(prec_p)	(prec_p)->prec_mach_p
#define SET_PREC_CODE(prec_p,c)		(prec_p)->prec_code = c
#define SET_PREC_CODE_BITS(prec_p,b)	(prec_p)->prec_code |= b
#define CLEAR_PREC_CODE_BITS(prec_p,b)	(prec_p)->prec_code &= (b)

#define SET_PREC_SIZE(prec_p,s)		(prec_p)->prec_size = s
#define SET_PREC_MACH_PREC_PTR(prec_p,p)	(prec_p)->prec_mach_p = p

#define PREC_MACH_CODE(prec_p)		PREC_CODE(PREC_MACH_PREC_PTR(prec_p))
#define PREC_MACH_SIZE(prec_p)		PREC_SIZE(PREC_MACH_PREC_PTR(prec_p))

#define PREC_FOR_CODE(p)		prec_for_code(p)

#define NAME_FOR_PREC_CODE(p)		PREC_NAME(PREC_FOR_CODE(p))
#define SIZE_FOR_PREC_CODE(p)		PREC_SIZE(PREC_FOR_CODE(p))


typedef struct shape_info {
	Dimension_Set *		si_mach_dims;
	Dimension_Set *		si_type_dims;
	Increment_Set *		si_mach_incs;
	Increment_Set *		si_type_incs;
	Precision *		si_prec_p;
	int32_t			si_maxdim;
	int32_t			si_mindim;
	int32_t			si_range_maxdim;
	int32_t			si_range_mindim;
	shape_flag_t		si_flags;
	/*int32_t			si_last_subi; */
} Shape_Info;

#define NO_SHAPE ((Shape_Info *) NULL)

/* Shape macros */

#define VOID_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_VOID )
#define IMAGE_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_IMAGE )
#define BITMAP_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_BIT )
#define VECTOR_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_VECTOR )
#define COLVEC_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_COLVEC )
#define ROWVEC_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_ROWVEC )
#define SEQUENCE_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_SEQUENCE )
#define HYPER_SEQ_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_HYPER_SEQ )
/* BUG?  is this correct for complex/quaternion??? */
#define SCALAR_SHAPE( shpp )		( ( SHP_FLAGS( shpp ) & DT_SCALAR ) && ( DIMENSION(SHP_TYPE_DIMS(shpp),0) == 1 ) )
#define PIXEL_SHAPE( shpp )		( ( SHP_FLAGS( shpp ) & DT_SCALAR ) )
#define UNKNOWN_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_UNKNOWN_SHAPE )
#define INTERLACED_SHAPE( shpp )	( SHP_FLAGS( shpp ) & DT_INTERLACED )

#define COMPLEX_SHAPE( shpp )		( COMPLEX_PRECISION( SHP_PREC(shpp) ) )
#define QUAT_SHAPE( shpp )		( QUAT_PRECISION( SHP_PREC(shpp) ) )
#define REAL_SHAPE( shpp )		( (SHP_FLAGS( shpp ) & (SHAPE_TYPE_MASK&~DT_MULTIDIM)) == 0 )
#define MULTIDIM_SHAPE( shpp )		( SHP_FLAGS( shpp ) & DT_MULTIDIM )

#define SHAPE_DIM_MASK	(DT_UNKNOWN_SHAPE|DT_HYPER_SEQ|DT_SEQUENCE|DT_IMAGE|DT_VECTOR|DT_SCALAR)
/* don't care about unsigned... */
#define SHAPE_TYPE_MASK	(DT_COMPLEX|DT_MULTIDIM|DT_QUAT)
#define SHAPE_MASK	(SHAPE_DIM_MASK|SHAPE_TYPE_MASK)

/* ShapeInfo macros */
#define COPY_DIMS(dsp1,dsp2)	*dsp1 = *dsp2
#define COPY_INCS(isp1,isp2)	*isp1 = *isp2

/* This line was put in COPY_SHAPE for extra debugging... */
/*show_shape_addrs("dst",dst_shpp); show_shape_addrs("src",src_shpp); */

// COPY_SHAPE used to be a simple structure copy, but now the shape
// contains pointers to stuff...
// We could encapsulate the local data into a struct?

#define COPY_SHAPE(dst_shpp,src_shpp)	{				\
									\
	SET_SHP_PREC_PTR(dst_shpp, SHP_PREC_PTR(src_shpp) );		\
	SET_SHP_MAXDIM(dst_shpp, SHP_MAXDIM(src_shpp) );		\
	SET_SHP_MINDIM(dst_shpp, SHP_MINDIM(src_shpp) );		\
	SET_SHP_RANGE_MAXDIM(dst_shpp, SHP_RANGE_MAXDIM(src_shpp) );	\
	SET_SHP_RANGE_MINDIM(dst_shpp, SHP_RANGE_MINDIM(src_shpp) );	\
	SET_SHP_FLAGS(dst_shpp, SHP_FLAGS(src_shpp) );			\
	/*SET_SHP_LAST_SUBI(dst_shpp, SHP_LAST_SUBI(src_shpp) );*/	\
	COPY_DIMS((SHP_TYPE_DIMS(dst_shpp)),(SHP_TYPE_DIMS(src_shpp)));	\
	COPY_DIMS((SHP_MACH_DIMS(dst_shpp)),(SHP_MACH_DIMS(src_shpp)));	\
	COPY_INCS((SHP_TYPE_INCS(dst_shpp)),(SHP_TYPE_INCS(src_shpp)));	\
	COPY_INCS((SHP_MACH_INCS(dst_shpp)),(SHP_MACH_INCS(src_shpp)));	\
	}

#define NEW_SHAPE_INFO		((Shape_Info *)getbuf(sizeof(Shape_Info)))
#define SHP_PREC(shp)		PREC_CODE( (shp)->si_prec_p )
#define SHP_PREC_PTR(shp)	(shp)->si_prec_p
/* We may not want to diddle these bits - because now the precisions */
/* are pointed-to structures?!  BUG? */
#define SET_SHP_PREC_BITS(shp,b)	SET_PREC_CODE_BITS( (shp)->si_prec_p, b )
#define CLEAR_SHP_PREC_BITS(shp,b)	CLEAR_PREC_CODE_BITS( (shp)->si_prec_p, b )
#define SHP_MACH_PREC_PTR(shp)	(shp)->si_prec_p->prec_mach_p
#define SHP_PREC_MACH_SIZE(shp)	PREC_MACH_SIZE(shp->si_prec_p)
#define SHP_MAXDIM(shp)		(shp)->si_maxdim
#define SHP_MINDIM(shp)		(shp)->si_mindim
#define SHP_RANGE_MAXDIM(shp)	(shp)->si_range_maxdim
#define SHP_RANGE_MINDIM(shp)	(shp)->si_range_mindim
#define SHP_MACH_DIMS(shp)	(shp)->si_mach_dims
#define SHP_TYPE_DIMS(shp)	(shp)->si_type_dims
#define SHP_TYPE_DIM(shp,idx)	DS_DIM((shp)->si_type_dims,idx)
#define SHP_MACH_DIM(shp,idx)	DS_DIM((shp)->si_mach_dims,idx)
#define SHP_N_MACH_ELTS(shpp)	DS_N_ELTS((shpp)->si_mach_dims)
#define SHP_N_TYPE_ELTS(shpp)	DS_N_ELTS((shpp)->si_type_dims)
//#define SHP_LAST_SUBI(shpp)	(shpp)->si_last_subi
#define SHP_FLAGS(shpp)		(shpp)->si_flags
#define SET_SHP_FLAGS(shpp,f)	(shpp)->si_flags = f

#define SHP_SEQS(shpp)		DS_SEQS(   (shpp)->si_type_dims )
#define SHP_FRAMES(shpp)	DS_FRAMES( (shpp)->si_type_dims )
#define SHP_ROWS(shpp)		DS_ROWS(   (shpp)->si_type_dims )
#define SHP_COLS(shpp)		DS_COLS(   (shpp)->si_type_dims )
#define SHP_COMPS(shpp)		DS_COMPS(  (shpp)->si_type_dims )
#define SHP_DIMENSION(shpp,idx)	DIMENSION( (shpp)->si_type_dims, idx )

#define SET_SHP_SEQS(shpp,v)	SET_DS_SEQS(   (shpp)->si_type_dims,v)
#define SET_SHP_FRAMES(shpp,v)	SET_DS_FRAMES( (shpp)->si_type_dims, v)
#define SET_SHP_ROWS(shpp,v)	SET_DS_ROWS(   (shpp)->si_type_dims, v )
#define SET_SHP_COLS(shpp,v)	SET_DS_COLS(   (shpp)->si_type_dims, v )
#define SET_SHP_COMPS(shpp,v)	SET_DS_COMPS(  (shpp)->si_type_dims, v)

#define DS_SEQS(dsp)		DIMENSION(dsp,4)
#define DS_FRAMES(dsp)		DIMENSION(dsp,3)
#define DS_ROWS(dsp)		DIMENSION(dsp,2)
#define DS_COLS(dsp)		DIMENSION(dsp,1)
#define DS_COMPS(dsp)		DIMENSION(dsp,0)

#define SET_DS_SEQS(dsp,v)	SET_DIMENSION(dsp,4,v)
#define SET_DS_FRAMES(dsp,v)	SET_DIMENSION(dsp,3,v)
#define SET_DS_ROWS(dsp,v)	SET_DIMENSION(dsp,2,v)
#define SET_DS_COLS(dsp,v)	SET_DIMENSION(dsp,1,v)
#define SET_DS_COMPS(dsp,v)	SET_DIMENSION(dsp,0,v)

#define SHP_MACH_INCS(shp)		(shp)->si_mach_incs
#define SHP_TYPE_INCS(shp)		(shp)->si_type_incs
#define SET_SHP_MACH_INCS(shp,isp)	(shp)->si_mach_incs = isp
#define SET_SHP_TYPE_INCS(shp,isp)	(shp)->si_type_incs = isp
#define SHP_MACH_INC(shp,idx)		(shp)->si_mach_incs->is_increment[idx]
#define SHP_TYPE_INC(shp,idx)		(shp)->si_type_incs->is_increment[idx]
#define SET_SHP_MACH_INC(shp,idx,v)	(shp)->si_mach_incs->is_increment[idx] = v
#define SET_SHP_TYPE_INC(shp,idx,v)	(shp)->si_type_incs->is_increment[idx] = v

#define SET_SHP_MACH_DIMS(shp,dsp)	(shp)->si_mach_dims = dsp
#define SET_SHP_TYPE_DIMS(shp,dsp)	(shp)->si_type_dims = dsp
#define SET_SHP_TYPE_DIM(shp,idx,v)	SET_DS_DIM((shp)->si_type_dims,idx,v)
#define SET_SHP_MACH_DIM(shp,idx,v)	SET_DS_DIM((shp)->si_mach_dims,idx,v)
#define SET_SHP_PREC_PTR(shp,prec_p)	(shp)->si_prec_p = prec_p
#define SET_SHP_MAXDIM(shp,v)		(shp)->si_maxdim = v
#define SET_SHP_MINDIM(shp,v)		(shp)->si_mindim = v
#define SET_SHP_RANGE_MAXDIM(shp,v)	(shp)->si_range_maxdim = v
#define SET_SHP_RANGE_MINDIM(shp,v)	(shp)->si_range_mindim = v
#define SET_SHP_N_MACH_ELTS(shp,v)	SET_DS_N_ELTS((shp)->si_mach_dims,v)
#define SET_SHP_N_TYPE_ELTS(shp,v)	SET_DS_N_ELTS((shp)->si_type_dims,v)
//#define SET_SHP_LAST_SUBI(shp,v)	(shp)->si_last_subi = v
#define SET_SHP_FLAG_BITS(shp,v)	(shp)->si_flags |= v
#define CLEAR_SHP_FLAG_BITS(shp,v)	(shp)->si_flags &= ~(v)

#endif /* ! _SHAPE_INFO_H_ */

