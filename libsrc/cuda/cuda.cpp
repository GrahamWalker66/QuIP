
/* jbm's interface to cuda devices */

/* This file contains the menu-callable functions, which in turn call
 * host functions which are typed and take an oap argument.
 * These host functions then call the gpu kernel functions...
 */

#include "quip_config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

//#ifdef HAVE_CUDA
// This is used in cuda 4
//#include <cutil_inline.h>

#ifdef HAVE_CUDA
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <curand.h>
#define BUILD_FOR_CUDA
#endif // HAVE_CUDA

#include "quip_prot.h"
#include "my_cuda.h"
#include "cuda_supp.h"

// global var
int max_threads_per_block;

Cuda_Device *curr_cdp=NO_CUDA_DEVICE;

Data_Area *cuda_data_area[MAX_CUDA_DEVICES][N_CUDA_DEVICE_AREAS];

#ifdef HAVE_CUDA
#define DEFAULT_CUDA_DEV_VAR	"DEFAULT_CUDA_DEVICE"


#endif // HAVE_CUDA

ITEM_INTERFACE_DECLARATIONS( Cuda_Device, cudev )

#define PICK_CUDEV(pmpt)	pick_cudev(QSP_ARG  pmpt)


/* On the host 1L<<33 gets us bit 33 - but 1<<33 does not,
 * because, by default, ints are 32 bits.  We don't know
 * how nvcc treats 1L...  but we can try it...
 */

#ifdef HAVE_CUDA

#ifdef FOOBAR
static const char * available_cuda_device_name(QSP_ARG_DECL  const char *name,char *scratch_string)
{
	Cuda_Device *cdp;
	const char *s;
	int n=1;

	s=name;
	while(n<=MAX_CUDA_DEVICES){
		cdp = cudev_of(QSP_ARG  s);
		if( cdp == NO_CUDA_DEVICE ) return(s);

		// This name is in use
		n++;
		sprintf(scratch_string,"%s_%d",name,n);
		s=scratch_string;
	}
	sprintf(ERROR_STRING,"Number of %s CUDA devices exceed configured maximum %d!?",
		name,MAX_CUDA_DEVICES);
	WARN(ERROR_STRING);
	ERROR1(ERROR_STRING);
	return(NULL);	// NOTREACHED - quiet compiler
}
#endif // FOOBAR
#endif // HAVE_CUDA

#ifdef USE_OLD_CODE
#ifdef HAVE_CUDA
static void init_cuda_device(QSP_ARG_DECL  int index)
{
	cudaDeviceProp deviceProp;
	cudaError_t e;
	Cuda_Device *cdp;
	char name[LLEN];
	char cname[LLEN];
	const char *name_p;
	char *s;
	Data_Area *ap;
	float comp_cap;

	if( index >= MAX_CUDA_DEVICES ){
		sprintf(ERROR_STRING,"Program is compiled for a maximum of %d CUDA devices, can't inititialize device %d.",
			MAX_CUDA_DEVICES,index);
		ERROR1(ERROR_STRING);
	}

	if( verbose ){
		sprintf(ERROR_STRING,"init_cuda_device %d BEGIN",index);
		advise(ERROR_STRING);
	}

	if( (e=cudaGetDeviceProperties(&deviceProp, index)) != cudaSuccess ){
		describe_cuda_driver_error2("init_cuda_device","cudaGetDeviceProperties",e);
		return;
	}

	if (deviceProp.major == 9999 && deviceProp.minor == 9999){
		sprintf(ERROR_STRING,"There is no CUDA device with dev = %d!?.\n",index);
		WARN(ERROR_STRING);

		/* What should we do here??? */
		return;
	}

	/* Put the compute capability into a script variable so that we can use it */
	comp_cap = deviceProp.major * 10 + deviceProp.minor;
	if( comp_cap > CUDA_COMP_CAP ){
		sprintf(ERROR_STRING,"init_cuda_device:  CUDA device %s has compute capability %d.%d, but program was configured for %d.%d!?",
			deviceProp.name,deviceProp.major,deviceProp.minor,
			CUDA_COMP_CAP/10,CUDA_COMP_CAP%10);
		WARN(ERROR_STRING);
	}

	/* BUG if there are multiple devices, we need to make sure that this is set
	 * correctly for the current context!?
	 */
	sprintf(ERROR_STRING,"%d.%d",deviceProp.major,deviceProp.minor);
	assign_var(QSP_ARG  "cuda_comp_cap",ERROR_STRING);


	/* What does this do??? */
	e = cudaSetDeviceFlags( cudaDeviceMapHost );
	if( e != cudaSuccess ){
		describe_cuda_driver_error2("init_cuda_device",
			"cudaSetDeviceFlags",e);
	}

	strcpy(name,deviceProp.name);

	/* change spaces to underscores */
	s=name;
	while(*s){
		if( *s==' ' ) *s='_';
		s++;
	}

	/* We might have two of the same devices installed in a single system.
	 * In this case, we can't use the device name twice, because there will
	 * be a conflict.  The first one gets the name, then we have to check and
	 * make sure that the name is not in use already.  If it is, then we append
	 * a number to the string...
	 */
	name_p = available_cuda_device_name(QSP_ARG  name,cname);	// use cname as scratch string
	cdp = new_cudev(QSP_ARG  name_p);

#ifdef CAUTIOUS
	if( cdp == NO_CUDA_DEVICE ){
		sprintf(ERROR_STRING,"CAUTIOUS:  init_cuda_device:  Error creating cuda device struct for %s!?",name_p);
		WARN(ERROR_STRING);
		return;
	}
#endif /* CAUTIOUS */

	/* Remember this name in case the default is not found */
	if( first_cuda_dev_name == NULL )
		first_cuda_dev_name = cdp->cudev_name;

	/* Compare this name against the default name set in
	 * the environment, if it exists...
	 */
	if( default_cuda_dev_name != NULL && ! default_cuda_dev_found ){
		if( !strcmp(cdp->cudev_name,default_cuda_dev_name) )
			default_cuda_dev_found=1;
	}

	cdp->cudev_index = index;
	cdp->cudev_prop = deviceProp;

	set_cuda_device(cdp);	// is this call just so we can call cudaMalloc?

	// address set to NULL says use custom allocator - see dobj/makedobj.c

fprintf(stderr,"init_cuda_device calling init_area()\n");
	ap = area_init(QSP_ARG  name_p,NULL,0, MAX_CUDA_GLOBAL_OBJECTS,DA_CUDA_GLOBAL);
	if( ap == NO_AREA ){
		sprintf(ERROR_STRING,
	"init_cuda_device:  error creating global data area %s",name_p);
		WARN(ERROR_STRING);
	}
	// g++ won't take this line!?
	SET_AREA_CUDA_DEV(ap,cdp);
	//set_device_for_area(ap,cdp);

	cuda_data_area[index][CUDA_GLOBAL_AREA_INDEX] = ap;

	/* We used to declare a heap for constant memory here,
	 * but there wasn't much of a point because:
	 * Constant memory can't be allocated, rather it is declared
	 * in the .cu code, and placed by the compiler as it sees fit.
	 * To have objects use this, we would have to declare a heap and
	 * manage it ourselves...
	 * There's only 64k, so we should be sparing...
	 * We'll try this later...
	 */


	/* Make up another area for the host memory
	 * which is locked and mappable to the device.
	 * We don't allocate a pool here, but do it as needed...
	 */

	strcpy(cname,name_p);
	strcat(cname,"_host");
	ap = area_init(QSP_ARG  cname,(u_char *)NULL,0,MAX_CUDA_MAPPED_OBJECTS,
								DA_CUDA_HOST);
	if( ap == NO_AREA ){
		sprintf(ERROR_STRING,
	"init_cuda_device:  error creating host data area %s",cname);
		ERROR1(ERROR_STRING);
	}
	SET_AREA_CUDA_DEV(ap, cdp);
	cuda_data_area[index][CUDA_HOST_AREA_INDEX] = ap;

	/* Make up another psuedo-area for the mapped host memory;
	 * This is the same memory as above, but mapped to the device.
	 * In the current implementation, we create objects in the host
	 * area, and then automatically create an alias on the device side.
	 * There is a BUG in that by having this psuedo area in the data
	 * area name space, a user could select it as the data area and
	 * then try to create an object.  We will detect this in make_dobj,
	 * and complain.
	 */

	strcpy(cname,name_p);
	strcat(cname,"_host_mapped");
	ap = area_init(QSP_ARG  cname,(u_char *)NULL,0,MAX_CUDA_MAPPED_OBJECTS,
							DA_CUDA_HOST_MAPPED);
	if( ap == NO_AREA ){
		sprintf(ERROR_STRING,
	"init_cuda_device:  error creating host-mapped data area %s",cname);
		ERROR1(ERROR_STRING);
	}
	SET_AREA_CUDA_DEV(ap,cdp);
	cuda_data_area[index][CUDA_HOST_MAPPED_AREA_INDEX] = ap;


	/* Restore the normal area */
	set_data_area(cuda_data_area[index][CUDA_GLOBAL_AREA_INDEX]);

	if( verbose ){
		sprintf(ERROR_STRING,"init_cuda_device %d DONE",index);
		advise(ERROR_STRING);
	}
}

// c++ version

void _init_cuda_devices(SINGLE_QSP_ARG_DECL)
{
	int n_devs,i;

	/* We don't get a proper error message if the cuda files in /dev
	 * are not readable...  So we check that first.
	 */

	check_file_access(QSP_ARG  "/dev/nvidiactl");

	cudaGetDeviceCount(&n_devs);

	if( n_devs == 0 ){
		WARN("No CUDA devices found!?");
		return;
	}

	if( verbose ){
		sprintf(ERROR_STRING,"%d cuda devices found...",n_devs);
		advise(ERROR_STRING);
	}

	default_cuda_dev_name = getenv(DEFAULT_CUDA_DEV_VAR);
	/* may be null */

	for(i=0;i<n_devs;i++){
		char s[32];

		sprintf(s,"/dev/nvidia%d",i);
		check_file_access(QSP_ARG  s);

		init_cuda_device(QSP_ARG  i);
	}

	if( default_cuda_dev_name == NULL ){
		/* Not specified in environment */
		assign_var(QSP_ARG  DEFAULT_CUDA_DEV_VAR,first_cuda_dev_name);
		default_cuda_dev_found=1;	// not really necessary?
	} else if( ! default_cuda_dev_found ){
		/* specified incorrectly */
		sprintf(ERROR_STRING, "%s %s not found.\nUsing device %s.",
			DEFAULT_CUDA_DEV_VAR,default_cuda_dev_name,
			first_cuda_dev_name);
		WARN(ERROR_STRING);

		assign_var(QSP_ARG  DEFAULT_CUDA_DEV_VAR,first_cuda_dev_name);
		default_cuda_dev_found=1;	// not really necessary?
	}

	/* hopefully the vector library is already initialized - can we be sure? */
	vl_init(SINGLE_QSP_ARG);
	set_gpu_dispatch_func(gpu_dispatch);
} // end init_cuda_devices
#endif // HAVE_CUDA
#endif // USE_OLD_CODE


// make these C so we can link from other C files...

#ifdef FOOBAR
extern "C" {

void gpu_mem_upload(QSP_ARG_DECL  void *dst, void *src, size_t siz )
{
#ifdef HAVE_CUDA
	cudaError_t error;

#ifdef OLD_CUDA4
	cutilSafeCall( cudaMemcpy(dst, src, siz, cudaMemcpyHostToDevice) );
#else
	error = cudaMemcpy(dst, src, siz, cudaMemcpyHostToDevice);
	if( error != cudaSuccess ){
		// BUG report cuda error
		WARN("Error in cudaMemcpy, host to device!?");
	}
#endif
#else // ! HAVE_CUDA
	NO_CUDA_MSG(mem_upload)
#endif // ! HAVE_CUDA
}

void gpu_mem_dnload(QSP_ARG_DECL  void *dst, void *src, size_t siz )
{
#ifdef HAVE_CUDA
	cudaError_t error;

        //cutilSafeCall( cutilDeviceSynchronize() );	// added for 4.0?
#ifdef OLD_CUDA4
	cutilSafeCall( cudaMemcpy(dst, src, siz, cudaMemcpyDeviceToHost) );
#else
	error = cudaMemcpy(dst, src, siz, cudaMemcpyDeviceToHost) ;
	if( error != cudaSuccess ){
		// BUG report cuda error
		WARN("Error in cudaMemcpy, device to host!?");
	}
#endif
#else // ! HAVE_CUDA
	NO_CUDA_MSG(mem_dnload)
#endif // ! HAVE_CUDA
}

void gpu_obj_upload(QSP_ARG_DECL  Data_Obj *dpto, Data_Obj *dpfr)
{
	size_t siz;

	CHECK_NOT_RAM("do_gpu_obj_upload","destination",dpto)
	CHECK_RAM("do_gpu_obj_upload","source",dpfr)
	CHECK_CONTIG_DATA("do_gpu_obj_upload","source",dpfr)
	CHECK_CONTIG_DATA("do_gpu_obj_upload","destination",dpto)
	CHECK_SAME_SIZE(dpto,dpfr,"do_gpu_obj_upload")
	CHECK_SAME_PREC(dpto,dpfr,"do_gpu_obj_upload")

#ifdef FOOBAR
	if( IS_BITMAP(dpto) )
		siz = BITMAP_WORD_COUNT(dpto) * PREC_SIZE( PREC_FOR_CODE(BITMAP_MACH_PREC) );
	else
		siz = OBJ_N_MACH_ELTS(dpto) * PREC_SIZE( OBJ_MACH_PREC_PTR(dpto) );
#endif /* FOOBAR */
	siz = OBJ_N_MACH_ELTS(dpto) * PREC_SIZE( OBJ_MACH_PREC_PTR(dpto) );

	gpu_mem_upload(QSP_ARG  OBJ_DATA_PTR(dpto), OBJ_DATA_PTR(dpfr), siz );
}

void gpu_obj_dnload(QSP_ARG_DECL  Data_Obj *dpto,Data_Obj *dpfr)
{
	size_t siz;

	CHECK_RAM("gpu_obj_dnload","destination",dpto)
	CHECK_NOT_RAM("gpu_obj_dnload","source",dpfr)
	CHECK_CONTIG_DATA("gpu_obj_dnload","source",dpfr)
	CHECK_CONTIG_DATA("gpu_obj_dnload","destination",dpto)
	CHECK_SAME_SIZE(dpto,dpfr,"gpu_obj_dnload")
	CHECK_SAME_PREC(dpto,dpfr,"gpu_obj_dnload")

	/* TEST - does this work for bitmaps? */
	siz = OBJ_N_MACH_ELTS(dpto) * PREC_SIZE( OBJ_PREC_PTR(dpto) );

	gpu_mem_dnload(QSP_ARG  OBJ_DATA_PTR(dpto), OBJ_DATA_PTR(dpfr), siz );
}

}	// end extern C


COMMAND_FUNC( do_gpu_obj_dnload )
{
	Data_Obj *dpto, *dpfr;

	dpto = PICK_OBJ("destination RAM object");
	dpfr = PICK_OBJ("source GPU object");

	if( dpto == NO_OBJ || dpfr == NO_OBJ ) return;

	gpu_obj_dnload(QSP_ARG  dpto,dpfr);
}

COMMAND_FUNC( do_gpu_obj_upload )
{
	Data_Obj *dpto, *dpfr;

	dpto = PICK_OBJ("destination GPU object");
	dpfr = PICK_OBJ("source RAM object");

	if( dpto == NO_OBJ || dpfr == NO_OBJ ) return;

	gpu_obj_upload(QSP_ARG  dpto,dpfr);
}


COMMAND_FUNC( do_gpu_fwdfft )
{
	CUDA_MENU_FUNC_DECLS
	const char *func_name="do_gpu_fwdfft";

	Data_Obj *dst_dp, *src1_dp;
	dst_dp = PICK_OBJ("destination object");
	src1_dp = PICK_OBJ("source object");

	if( dst_dp == NO_OBJ || src1_dp == NO_OBJ) return;

	CHECK_GPU_OBJ(dst_dp);
	CHECK_GPU_OBJ(src1_dp);

	CHECK_SAME_GPU(dst_dp, src1_dp)

	CHECK_CONTIGUITY(dst_dp);
	CHECK_CONTIGUITY(src1_dp);

	insure_cuda_device(dst_dp);

	//INSERT: Code that calls the fft function
	g_fwdfft(QSP_ARG  dst_dp, src1_dp);

	FINISH_CUDA_MENU_FUNC
}


// moved to veclib2

// a utility used in host_calls.h
#define MAXD(m,n)	(m>n?m:n)
#define MAX2(szi_p)	MAXD(szi_p->szi_dst_dim[i_dim],szi_p->szi_src_dim[1][i_dim])
#define MAX3(szi_p)	MAXD(MAX2(szi_p),szi_p->szi_src_dim[1][i_dim])

/* The setup_slow_len functions initialize the len variable (dim3),
 * and return the number of dimensions that are set.  We also need to
 * return which dimensions are used, so that we know which increments
 * to use (important for outer ops).
 *
 * We have a problem with complex numbers - tdim is 2 (in order that
 * we be able to index real and complex parts), but it is really one element.
 *
 * In general, we'd like to collapse contiguous dimensions.  For example,
 * in the case of a subimage of a 3-component image, we might collapse
 * the component and row dimensions into a single mega-row.  Otherwise
 * we need to support 3 lengths etc.
 *
 * When scanning multiple vectors, the length at each level needs to be the
 * maximum of lengths needed by the each of the vectors.  We use max_d to
 * hold this value.
 */

int setup_slow_len(dim3 *len_p,Size_Info *szi_p,dimension_t start_dim,int *dim_indices,int i_first,int n_vec)
{
	int i_dim;
	dimension_t max_d;
	int n_set=0;

	for(i_dim=start_dim;i_dim<N_DIMENSIONS;i_dim++){
		int i_src;

		/* Find the max len of all the objects at this level */
		max_d=DIMENSION(SZI_DST_DIMS(*szi_p),i_dim);
		for(i_src=i_first;i_src<(i_first+n_vec-1);i_src++)
			max_d=MAXD(max_d,DIMENSION(SZI_SRC_DIMS(*szi_p,i_src),i_dim));

		if( max_d > 1 ){
			if( n_set == 0 ){
				len_p->x = max_d;
				dim_indices[n_set] = i_dim;
				n_set ++;
			} else if ( n_set == 1 ){
				len_p->y = max_d;
				dim_indices[n_set] = i_dim;
				n_set ++;
			} else if( n_set == 2 ){
	/* CUDA compute capability 1.3 and below can't use 3-D grids; So here
	 * we need to conditionally complain if the number set is 3.
	 */
#if CUDA_COMP_CAP >= 20
				len_p->z = max_d;
				dim_indices[n_set] = i_dim;
				n_set ++;
#else /* CUDA_COMP_CAP < 20 */
				NWARN("Sorry, CUDA compute capability >= 2.0 required for 3-D array operations");
				return(-1);
#endif /* CUDA_COMP_CAP < 20 */
			} else {
				NWARN("Too many CUDA dimensions requested.");
				return(-1);
			}
		}
	}
	if( n_set == 0 ){
		len_p->x = len_p->y = len_p->z = 1;
		dim_indices[0] = dim_indices[1] = dim_indices[2] = (-1);
	} else if( n_set == 1 ){
		len_p->y = len_p->z = 1;
		dim_indices[1] = dim_indices[2] = (-1);
	} else if( n_set == 2 ){
		len_p->z = 1;
		dim_indices[2] = (-1);
	}
	return(n_set);
}


#include "enum_menu_calls.h"	// will lay out all the functions...

// BUG should have an enum file for the host calls too!

// The strategy for these kinds of operations is that we divide and conquer recursively.
// We have a global routine which operates on two pixels, and puts the result in a third.
//

H_CALL_ALL( vmaxv )
H_CALL_ALL( vminv )
H_CALL_S( vmxmv )
H_CALL_S( vmnmv )

H_CALL_ALL( vmaxi )
H_CALL_ALL( vmini )
H_CALL_S( vmxmi )
H_CALL_S( vmnmi )

H_CALL_ALL( vmaxg )
H_CALL_ALL( vming )
H_CALL_S( vmxmg )
H_CALL_S( vmnmg )

H_CALL_ALL( vsum )
H_CALL_ALL( vdot )
H_CALL_ALL( vramp1d )
H_CALL_ALL( vramp2d )

H_CALL_ALL( vvv_slct )
H_CALL_ALL( vvs_slct )
H_CALL_ALL( vss_slct )

H_CALL_ALL( vv_vv_lt )
H_CALL_ALL( vv_vv_gt )
H_CALL_ALL( vv_vv_le )
H_CALL_ALL( vv_vv_ge )
H_CALL_ALL( vv_vv_eq )
H_CALL_ALL( vv_vv_ne )

H_CALL_ALL( vv_vs_lt )
H_CALL_ALL( vv_vs_gt )
H_CALL_ALL( vv_vs_le )
H_CALL_ALL( vv_vs_ge )
H_CALL_ALL( vv_vs_eq )
H_CALL_ALL( vv_vs_ne )

H_CALL_ALL( vs_vv_lt )
H_CALL_ALL( vs_vv_gt )
H_CALL_ALL( vs_vv_le )
H_CALL_ALL( vs_vv_ge )
H_CALL_ALL( vs_vv_eq )
H_CALL_ALL( vs_vv_ne )

H_CALL_ALL( vs_vs_lt )
H_CALL_ALL( vs_vs_gt )
H_CALL_ALL( vs_vs_le )
H_CALL_ALL( vs_vs_ge )
H_CALL_ALL( vs_vs_eq )
H_CALL_ALL( vs_vs_ne )

H_CALL_ALL( ss_vv_lt )
H_CALL_ALL( ss_vv_gt )
H_CALL_ALL( ss_vv_le )
H_CALL_ALL( ss_vv_ge )
H_CALL_ALL( ss_vv_eq )
H_CALL_ALL( ss_vv_ne )

H_CALL_ALL( ss_vs_lt )
H_CALL_ALL( ss_vs_gt )
H_CALL_ALL( ss_vs_le )
H_CALL_ALL( ss_vs_ge )
H_CALL_ALL( ss_vs_eq )
H_CALL_ALL( ss_vs_ne )

H_CALL_MAP( vvm_lt )
H_CALL_MAP( vvm_gt )
H_CALL_MAP( vvm_le )
H_CALL_MAP( vvm_ge )
H_CALL_MAP( vvm_eq )
H_CALL_MAP( vvm_ne )

H_CALL_MAP( vsm_lt )
H_CALL_MAP( vsm_gt )
H_CALL_MAP( vsm_le )
H_CALL_MAP( vsm_ge )
H_CALL_MAP( vsm_eq )
H_CALL_MAP( vsm_ne )

H_CALL_F( vmgsq )
#endif // FOOBAR

//MENU_CALL_ALL( vsum )

COMMAND_FUNC( do_list_cudevs )
{
	list_cudevs(SINGLE_QSP_ARG);
}

#ifdef HAVE_CUDA
static void print_cudev_info(QSP_ARG_DECL  Cuda_Device *cdp)
{
	print_cudev_properties(QSP_ARG  cdp->cudev_index, &cdp->cudev_prop);
}
#endif // HAVE_CUDA

COMMAND_FUNC( do_cudev_info )
{
	Cuda_Device *cdp;

	cdp = PICK_CUDEV((char *)"device");
	if( cdp == NO_CUDA_DEVICE ) return;

#ifdef HAVE_CUDA
	print_cudev_info(QSP_ARG  cdp);
#else
	NO_CUDA_MSG(print_cudev_info)
#endif
}

void set_cuda_device( Cuda_Device *cdp )
{
#ifdef HAVE_CUDA
	cudaError_t e;

	if( curr_cdp == cdp ){
		sprintf(DEFAULT_ERROR_STRING,"set_cuda_device:  current device is already %s!?",cdp->cudev_name);
		NWARN(DEFAULT_ERROR_STRING);
		return;
	}

	e = cudaSetDevice( cdp->cudev_index );
	if( e != cudaSuccess )
		describe_cuda_driver_error2("set_cuda_device","cudaSetDevice",e);
	else
		curr_cdp = cdp;
#endif //  HAVE_CUDA
}

void insure_cuda_device( Data_Obj *dp )
{
	Cuda_Device *cdp;

	if( AREA_FLAGS(OBJ_AREA(dp)) & DA_RAM ){
		sprintf(DEFAULT_ERROR_STRING,
	"insure_cuda_device:  Object %s is a host RAM object!?",OBJ_NAME(dp));
		NWARN(DEFAULT_ERROR_STRING);
		return;
	}

	cdp = (Cuda_Device *) AREA_CUDA_DEV(OBJ_AREA(dp));

#ifdef CAUTIOUS
	if( cdp == NO_CUDA_DEVICE )
		NERROR1("CAUTIOUS:  null cuda device ptr in data area!?");
#endif /* CAUTIOUS */

	if( curr_cdp != cdp ){
sprintf(DEFAULT_ERROR_STRING,"insure_cuda_device:  curr_cdp = 0x%lx  cdp = 0x%lx",
(int_for_addr)curr_cdp,(int_for_addr)cdp);
NADVISE(DEFAULT_ERROR_STRING);

sprintf(DEFAULT_ERROR_STRING,"insure_cuda_device:  current device is %s, want %s",
curr_cdp->cudev_name,cdp->cudev_name);
NADVISE(DEFAULT_ERROR_STRING);
		set_cuda_device(cdp);
	}

}

void *tmpvec(int size,int len,const char *whence)
{
#ifdef HAVE_CUDA
	void *cuda_mem;
	cudaError_t drv_err;

	drv_err = cudaMalloc(&cuda_mem, size * len );
	if( drv_err != cudaSuccess ){
		sprintf(DEFAULT_MSG_STR,"tmpvec (%s)",whence);
		describe_cuda_driver_error2(DEFAULT_MSG_STR,"cudaMalloc",drv_err);
		NERROR1("CUDA memory allocation error");
	}

//sprintf(ERROR_STRING,"tmpvec:  %d bytes allocated at 0x%lx",len,(int_for_addr)cuda_mem);
//advise(ERROR_STRING);

//sprintf(ERROR_STRING,"tmpvec %s:  0x%lx",whence,(int_for_addr)cuda_mem);
//advise(ERROR_STRING);
	return(cuda_mem);
#else // ! HAVE_CUDA
	return NULL;
#endif // ! HAVE_CUDA
}

void freetmp(void *ptr,const char *whence)
{
#ifdef HAVE_CUDA
	cudaError_t drv_err;

//sprintf(ERROR_STRING,"freetmp %s:  0x%lx",whence,(int_for_addr)ptr);
//advise(ERROR_STRING);
	drv_err=cudaFree(ptr);
	if( drv_err != cudaSuccess ){
		sprintf(DEFAULT_MSG_STR,"freetmp (%s)",whence);
		describe_cuda_driver_error2(DEFAULT_MSG_STR,"cudaFree",drv_err);
	}
#endif // HAVE_CUDA
}

#ifdef HAVE_CUDA
//CUFFT
//static const char* getCUFFTError(cufftResult_t status)
static const char* getCUFFTError(cufftResult status)
{
	switch (status) {
		case CUFFT_SUCCESS:
			return "Success";
		case CUFFT_INVALID_PLAN:
			return "Invalid Plan";
		case CUFFT_ALLOC_FAILED:
			return "Allocation Failed";
		case CUFFT_INVALID_TYPE:
			return "Invalid Type";
		case CUFFT_INVALID_VALUE:
			return "Invalid Error";
		case CUFFT_INTERNAL_ERROR:
			return "Internal Error";
		case CUFFT_EXEC_FAILED:
			return "Execution Failed";
		case CUFFT_SETUP_FAILED:
			return "Setup Failed";
		case CUFFT_INVALID_SIZE:
			return "Invalid Size";
		case CUFFT_UNALIGNED_DATA:
			return "Unaligned data";
		// these were added later on iMac - present in older versions?
		// BUG - find correct version number...
#if CUDA_VERSION >= 5050
		case CUFFT_INCOMPLETE_PARAMETER_LIST:
			return "Incomplete parameter list";
		case CUFFT_INVALID_DEVICE:
			return "Invalid device";
		case CUFFT_PARSE_ERROR:
			return "Parse error";
		case CUFFT_NO_WORKSPACE:
			return "No workspace";
#endif
#if CUDA_VERSION >= 6050
		case CUFFT_NOT_IMPLEMENTED:
			return "Not implemented";
		case CUFFT_LICENSE_ERROR:
			return "License error";
#endif
	}
	sprintf(DEFAULT_ERROR_STRING,"Unexpected CUFFT return value:  %d",status);
	return(DEFAULT_ERROR_STRING);
}
#endif // HAVE_CUDA

void g_fwdfft(QSP_ARG_DECL  Data_Obj *dst_dp, Data_Obj *src1_dp)
{
#ifdef HAVE_CUDA
	//Variable declarations
	int NX = 256;
	//int BATCH = 10;
	int BATCH = 1;
	cufftResult_t status;

	//Declare plan for FFT
	cufftHandle plan;
	//cufftComplex *data;
	//cufftComplex *result;
	void *data;
	void *result;
	cudaError_t drv_err;

	//Allocate RAM
	//cutilSafeCall(cudaMalloc(&data, sizeof(cufftComplex)*NX*BATCH));	
	//cutilSafeCall(cudaMalloc(&result, sizeof(cufftComplex)*NX*BATCH));
	drv_err = cudaMalloc(&data, sizeof(cufftComplex)*NX*BATCH);
	if( drv_err != cudaSuccess ){
		WARN("error allocating cuda data buffer for fft!?");
		return;
	}
	drv_err = cudaMalloc(&result, sizeof(cufftComplex)*NX*BATCH);
	if( drv_err != cudaSuccess ){
		WARN("error allocating cuda result buffer for fft!?");
		// BUG clean up previous malloc...
		return;
	}

	//Create plan for FFT
	status = cufftPlan1d(&plan, NX, CUFFT_C2C, BATCH);
	if (status != CUFFT_SUCCESS) {
		sprintf(ERROR_STRING, "Error in cufftPlan1d: %s\n", getCUFFTError(status));
		NWARN(ERROR_STRING);
	}

	//Run forward fft on data
	status = cufftExecC2C(plan, (cufftComplex *)data,
			(cufftComplex *)result, CUFFT_FORWARD);
	if (status != CUFFT_SUCCESS) {
		sprintf(ERROR_STRING, "Error in cufftExecC2C: %s\n", getCUFFTError(status));
		NWARN(ERROR_STRING);
	}

	//Run inverse fft on data
	/*status = cufftExecC2C(plan, data, result, CUFFT_INVERSE);
	if (status != CUFFT_SUCCESS)
	{
		sprintf(ERROR_STRING, "Error in cufftExecC2C: %s\n", getCUFFTError(status));
		NWARN(ERROR_STRING);
	}*/

	//Free resources
	cufftDestroy(plan);
	cudaFree(data);
#else // ! HAVE_CUDA
	NO_CUDA_MSG(g_fwdfft)
#endif // ! HAVE_CUDA
}


typedef struct {
	const char *	ckpt_tag;
#ifdef HAVE_CUDA
	cudaEvent_t	ckpt_event;
#endif // HAVE_CUDA
} Cuda_Checkpoint;

static Cuda_Checkpoint *ckpt_tbl=NULL;
static int n_cuda_checkpoints=0;	// number of placements

#ifdef HAVE_CUDA

static int max_cuda_checkpoints=0;	// size of checkpoit table

static void init_cuda_checkpoints(int n)
{
	//CUresult e;
	cudaError_t drv_err;
	int i;

	if( max_cuda_checkpoints > 0 ){
		sprintf(DEFAULT_ERROR_STRING,
"init_cuda_checkpoints(%d):  already initialized with %d checpoints",
			n,max_cuda_checkpoints);
		NWARN(DEFAULT_ERROR_STRING);
		return;
	}
	ckpt_tbl = (Cuda_Checkpoint *) getbuf( n * sizeof(*ckpt_tbl) );
	if( ckpt_tbl == NULL ) NERROR1("failed to allocate checkpoint table");

	max_cuda_checkpoints = n;

	for(i=0;i<max_cuda_checkpoints;i++){
		drv_err=cudaEventCreate(&ckpt_tbl[i].ckpt_event);
		if( drv_err != cudaSuccess ){
			describe_cuda_driver_error2("init_cuda_checkpoints",
				"cudaEventCreate",drv_err);
			NERROR1("failed to initialize checkpoint table");
		}
		ckpt_tbl[i].ckpt_tag=NULL;
	}
}
#endif // HAVE_CUDA

COMMAND_FUNC( do_init_checkpoints )
{
	int n;

	n = HOW_MANY("maximum number of checkpoints");
#ifdef HAVE_CUDA
	init_cuda_checkpoints(n);
#else // ! HAVE_CUDA
	NO_CUDA_MSG(init_cuda_checkpoints)
#endif // ! HAVE_CUDA
}

#define CUDA_DRIVER_ERROR_RETURN(calling_funcname, cuda_funcname )	\
								\
	if( drv_err != cudaSuccess ){					\
		describe_cuda_driver_error2(calling_funcname,cuda_funcname,drv_err); \
		return;						\
	}


#define CUDA_ERROR_RETURN(calling_funcname, cuda_funcname )	\
								\
	if( e != CUDA_SUCCESS ){					\
		describe_cuda_error2(calling_funcname,cuda_funcname,e); \
		return;						\
	}


#define CUDA_ERROR_FATAL(calling_funcname, cuda_funcname )	\
								\
	if( e != CUDA_SUCCESS ){					\
		describe_cuda_error2(calling_funcname,cuda_funcname,e); \
		ERROR1("Fatal cuda error.");			\
	}


COMMAND_FUNC( do_set_checkpoint )
{
#ifdef HAVE_CUDA
	//cudaError_t e;
	cudaError_t drv_err;
	const char *s;

	s = NAMEOF("tag for this checkpoint");

	if( max_cuda_checkpoints == 0 ){
		NWARN("do_place_ckpt:  checkpoint table not initialized, setting to default size");
		init_cuda_checkpoints(256);
	}

	if( n_cuda_checkpoints >= max_cuda_checkpoints ){
		sprintf(ERROR_STRING,
	"do_place_ckpt:  Sorry, all %d checkpoints have already been placed",
			max_cuda_checkpoints);
		WARN(ERROR_STRING);
		return;
	}

	ckpt_tbl[n_cuda_checkpoints].ckpt_tag = savestr(s);

	// use default stream (0) for now, but will want to introduce
	// more streams later?
	drv_err = cudaEventRecord( ckpt_tbl[n_cuda_checkpoints++].ckpt_event, 0 );
	CUDA_DRIVER_ERROR_RETURN( "do_place_ckpt","cudaEventRecord")
#else // ! HAVE_CUDA
	NO_CUDA_MSG(do_set_checkpoint)
#endif // ! HAVE_CUDA
}

COMMAND_FUNC( do_show_checkpoints )
{
#ifdef HAVE_CUDA
	///*cudaError_t*/ CUresult e;
	cudaError_t drv_err;
	float msec, cum_msec;
	int i;

	if( n_cuda_checkpoints <= 0 ){
		NWARN("do_show_checkpoints:  no checkpoints placed!?");
		return;
	}

	drv_err = cudaEventSynchronize(ckpt_tbl[n_cuda_checkpoints-1].ckpt_event);
	CUDA_DRIVER_ERROR_RETURN("do_show_checkpoints", "cudaEventSynchronize")

	drv_err = cudaEventElapsedTime( &msec, ckpt_tbl[0].ckpt_event, ckpt_tbl[n_cuda_checkpoints-1].ckpt_event);
	CUDA_DRIVER_ERROR_RETURN("do_show_checkpoints", "cudaEventElapsedTime")
	sprintf(msg_str,"Total GPU time:\t%g msec",msec);
	prt_msg(msg_str);

	// show the start tag
	sprintf(msg_str,"GPU  %3d  %12.3f  %12.3f  %s",1,0.0,0.0,
		ckpt_tbl[0].ckpt_tag);
	prt_msg(msg_str);
	cum_msec =0.0;
	for(i=1;i<n_cuda_checkpoints;i++){
		drv_err = cudaEventElapsedTime( &msec, ckpt_tbl[i-1].ckpt_event,
			ckpt_tbl[i].ckpt_event);
		CUDA_DRIVER_ERROR_RETURN("do_show_checkpoints", "cudaEventElapsedTime")

		cum_msec += msec;
		sprintf(msg_str,"GPU  %3d  %12.3f  %12.3f  %s",i+1,msec,
			cum_msec, ckpt_tbl[i].ckpt_tag);
		prt_msg(msg_str);
	}
#else // ! HAVE_CUDA
	NO_CUDA_MSG(do_show_checkpoints)
#endif // ! HAVE_CUDA
}

COMMAND_FUNC( do_clear_checkpoints )
{
	int i;

	for(i=0;i<n_cuda_checkpoints;i++){
		rls_str(ckpt_tbl[i].ckpt_tag);
		ckpt_tbl[i].ckpt_tag=NULL;
	}
	n_cuda_checkpoints=0;
}




//#endif /* HAVE_CUDA */

