
#include "quip_config.h"
#include <stdio.h>
#include "pf_viewer.h"	// has to come first to pick up glew.h first
#include "quip_prot.h"
#include "platform.h"
#include "ocl_platform.h"


Platform_Device *curr_pdp=NULL;

static COMMAND_FUNC( do_list_pfs )
{
	list_platforms(SINGLE_QSP_ARG);
}

static COMMAND_FUNC( do_list_pfdevs )
{
	Compute_Platform *cpp;

	cpp = PICK_PLATFORM("");
	if( cpp == NO_PLATFORM ) return;

	// should we list devices for a single platform?
	//push_pfdev_context(QSP_ARG  PF_CONTEXT(cpp) );
	list_item_context(QSP_ARG  PF_CONTEXT(cpp));
	//if( pop_pfdev_context(SINGLE_QSP_ARG) == NO_ITEM_CONTEXT )
	//	ERROR1("do_list_pfdevs:  Failed to pop platform device context!?");
}

void select_pfdev( QSP_ARG_DECL  Platform_Device *pdp )
{
	curr_pdp = pdp;	// select_pfdef

	// How do we specify host-mapped objects???  BUG!
	set_data_area( PFDEV_AREA(pdp,PFDEV_GLOBAL_AREA_INDEX) );
}

static COMMAND_FUNC( do_select_pfdev )
{
	Platform_Device *pdp;
	Compute_Platform *cpp;

	cpp = PICK_PLATFORM("");

	if( cpp == NO_PLATFORM ){
		const char *s;
		s=NAMEOF("dummy word");
		// Now use it just to suppress a compiler warning...
		sprintf(ERROR_STRING,"Ignoring \"%s\"",s);
		advise(ERROR_STRING);
		return;
	}

	push_pfdev_context( QSP_ARG  PF_CONTEXT(cpp) );
	pdp = PICK_PFDEV("");
	pop_pfdev_context( SINGLE_QSP_ARG );

	if( pdp == NO_PFDEV ) return;
	select_pfdev(QSP_ARG  pdp);
}

static COMMAND_FUNC( do_obj_dnload )
{
	Data_Obj *dpto, *dpfr;

	dpto = PICK_OBJ("destination RAM object");
	dpfr = PICK_OBJ("source GPU object");

	if( dpto == NO_OBJ || dpfr == NO_OBJ ) return;

	//ocl_obj_dnload(QSP_ARG  dpto,dpfr);
	gen_obj_dnload(QSP_ARG  dpto, dpfr);
}

static COMMAND_FUNC( do_obj_upload )
{
	Data_Obj *dpto, *dpfr;

	dpto = PICK_OBJ("destination GPU object");
	dpfr = PICK_OBJ("source RAM object");

	if( dpto == NO_OBJ || dpfr == NO_OBJ ) return;

	//ocl_obj_upload(QSP_ARG  dpto,dpfr);
	gen_obj_upload(QSP_ARG  dpto,dpfr);
}

static COMMAND_FUNC(do_show_pfdev)
{
	if( curr_pdp == NO_PFDEV ){
		advise("No platform device selected.");
	} else {
		sprintf(ERROR_STRING,"Current platform device is:  %s (%s)",
			PFDEV_NAME(curr_pdp),
			PLATFORM_NAME(PFDEV_PLATFORM(curr_pdp))
			);
		advise(ERROR_STRING);
	}
}

static Platform_Device *find_pfdev( QSP_ARG_DECL  platform_type typ )
{
	List *lp;
	Node *np;
	Platform_Device *pdp;

	lp = pfdev_list(SINGLE_QSP_ARG);
	if( lp == NO_LIST ) return NO_PFDEV;
	np = QLIST_HEAD(lp);
	while( np != NO_NODE ){
		pdp = (Platform_Device *) NODE_DATA(np);
		if( PF_TYPE( PFDEV_PLATFORM(pdp) ) == typ ) return pdp;
		np = NODE_NEXT(np);
	}
	return NO_PFDEV;
}

static const char *dev_type_names[]={"cuda","openCL"};
#define N_DEVICE_TYPES	(sizeof(dev_type_names)/sizeof(char *))

static COMMAND_FUNC(do_set_dev_type)
{
	int i;
	Platform_Device *pdp=NO_PFDEV;

	i=WHICH_ONE( "software interface",N_DEVICE_TYPES , dev_type_names);
	if( i < 0 ) return;

	if( i == 0 )
		pdp = find_pfdev(QSP_ARG  PLATFORM_CUDA);
	else if( i == 1 )
		pdp = find_pfdev(QSP_ARG  PLATFORM_OPENCL);

	if( pdp == NO_PFDEV ){
		sprintf(ERROR_STRING,"No %s device found!?",dev_type_names[i]);
		WARN(ERROR_STRING);
		return;
	}

	sprintf(ERROR_STRING,"Using %s device %s.",dev_type_names[i],PFDEV_NAME(pdp));
	advise(ERROR_STRING);

	select_pfdev(QSP_ARG  pdp);
}

static COMMAND_FUNC( do_pfdev_info )
{
	Platform_Device *pdp;

	pdp = PICK_PFDEV("");
	if( pdp == NO_PFDEV ) return;

	(* PF_DEVINFO_FN(PFDEV_PLATFORM(pdp)))(QSP_ARG  pdp);
}

static COMMAND_FUNC( do_pf_info )
{
	Compute_Platform *cdp;

	cdp = PICK_PLATFORM("");
	if( cdp == NO_PLATFORM ) return;

	sprintf(MSG_STR,"Platform name:  %s",PLATFORM_NAME(cdp));
	prt_msg(MSG_STR);

	(* PF_INFO_FN(cdp))(QSP_ARG  cdp);
}


#define ADD_CMD(s,f,h)	ADD_COMMAND(platform_menu,s,f,h)

MENU_BEGIN(platform)
//ADD_CMD( list,		do_list_platforms,	list available platforms )
// should we have a platform info command?
ADD_CMD( list,		do_list_pfs,		list platforms )
ADD_CMD( info,		do_pf_info,		print device information )
ADD_CMD( list_devices,	do_list_pfdevs,		list devices )
ADD_CMD( device_info,	do_pfdev_info,		print device information )
ADD_CMD( select,	do_select_pfdev,	select platform/device )
ADD_CMD( device_type,	do_set_dev_type,	use device of specified type )
ADD_CMD( show,		do_show_pfdev,		show current default platform )
ADD_CMD( upload,	do_obj_upload,		upload a data object to a device )
ADD_CMD( dnload,	do_obj_dnload,		download a data object from a device )
#ifdef FOOBAR
// To remove dependencies, these functions should go
// into the viewmenu library...
#ifndef BUILD_FOR_OBJC
ADD_CMD( viewer,	do_new_pf_vwr,		create a new platform viewer )
// gl_buffer moved to opengl menu
ADD_CMD( gl_buffer,	do_new_gl_buffer,	create a new GL buffer )
ADD_CMD( load,		do_load_pf_vwr,		load platform viewer with an image )
#endif // ! BUILD_FOR_OBJC
#endif // FOOBAR
MENU_END(platform)

// We use ifdef's to decide which platforms to initialize here...
// That makes it difficult to include this function in a program
// That doesn't need OpenCL or Cuda...
// But we don't know how to have these platforms register
// themselves, so we can't take this out...

void init_all_platforms(SINGLE_QSP_ARG_DECL)
{
	static int inited=0;
	if( inited ) return;

	vl2_init_platform(SINGLE_QSP_ARG);
#ifdef HAVE_OPENCL
	ocl_init_platform(SINGLE_QSP_ARG);
#endif // HAVE_OPENCL
#ifdef HAVE_CUDA
	cu2_init_platform(SINGLE_QSP_ARG);
#endif // HAVE_CUDA

	inited=1;
}

COMMAND_FUNC( do_platform_menu )
{
	static int inited=0;
	if( ! inited ){
		init_all_platforms(SINGLE_QSP_ARG);
		inited=1;
	}
	PUSH_MENU(platform);
}

