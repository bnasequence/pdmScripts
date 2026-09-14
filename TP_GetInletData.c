#include "udf.h"

DEFINE_EXECUTE_ON_LOADING(tpInletOnLoading, libname)
{
#if !RP_NODE
	Message(
"\tHOST: 1) Set the rp-variable \"tp-inletid\" using the following console command:\n\n\
\t\t (rp-var-define 'tp-inletid INLET_ID 'integer #f),\n\n\
\twhere INLET_ID equals the inlet cell zone id (see Cell Zones in the structure tree).\n\n\
\tHOST: 2) Run \"tpInletGetData\" UDF by going to User-Defined > Execute on Demand\n\
\tin order to print the inlet data to the console.\n"
	);
#endif
}

DEFINE_ON_DEMAND(tpInletGetData)
{	
	Domain	*d;
	int		pe, inletID;
#if !RP_HOST
	face_t	f;	
	float	x[3];
	Thread	*t;
#endif
	
#if !RP_NODE
	if(RP_Variable_Exists_P("tp-inletid"))
	{
		inletID = RP_Get_Integer("tp-inletid");
	}
	else
	{
		Message(
"\tHOST: WARNING! You must set the rp-variable \"tp-inletid\" using the following console command:\n\n\
\t\t (rp-var-define 'tp-inletid INLET_ID 'integer #f),\n\n\
\twhere INLET_ID equals the inlet cell zone id (see Cell Zones in the structure tree).\n"
		);
		inletID = -1;
	}
	PRF_CSEND_INT(node_zero, &inletID, 1, node_host);
#endif
#if	!RP_HOST
	if(I_AM_NODE_ZERO_P)
	{
		PRF_CRECV_INT(node_host, &inletID, 1, node_host);
		compute_node_loop_not_zero(pe)
		{
			PRF_CSEND_INT(pe, &inletID, 1, node_zero);
		}
	}
	else
	{
		PRF_CRECV_INT(node_zero, &inletID, 1, node_zero);
	}

	d = Get_Domain(1);	
	t = Lookup_Thread(d, inletID);	
	begin_f_loop(f, t)
	{
		F_CENTROID(x, f, t);
		Message("%d,%d,%.6f,%.6f,%.6f\n", myid, f, x[0], x[1], x[2]);	
	}
	end_f_loop(f, t)
#endif	
}