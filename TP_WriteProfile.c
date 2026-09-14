#include "udf.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define	buffer	1024
#define	size	6

bool		coordsWritten = false;
const char	*filePrefix;
int			filePrecision;
int			zoneID;

DEFINE_EXECUTE_ON_LOADING(tpWriteOnLoading, libname)
{
#if !RP_NODE
	Message(
"\n\tHOST: 1) Create the \"tp_pre\" folder in your working directory.\n\
\tHOST: 2) Set the rp-variable \"tp-zoneid\" using the following console command:\n\n\
\t\t (rp-var-define 'tp-zoneid ZONE_ID 'integer #f),\n\n\
\twhere ZONE_ID equals the sampling plane cell zone id (see Cell Zones in the structure tree).\n\
\tYou can change profile prefix by defining \"tp-prefix\" string variable (default: \"TP\").\n\
\tYou can also change timestep precision by defining \"tp-precision\" integer variable (default: 6 digits).\n\
\tHOST: 3) Run \"tpWriteInit\" UDF by going to User-Defined > Execute on Demand\n\
\tin order to initialise the variables used by the tpWrite scripts.\n\
\tHOST: 4) Finally, hook \"tpWriteAtEnd\" UDF by going to User-Defined > Function Hooks... > Execute at End.\n\n"
	);
#endif
}

DEFINE_ON_DEMAND(tpWriteInit)
{
	int		pe;
	
	coordsWritten = false;
	
#if !RP_NODE
	if(RP_Variable_Exists_P("tp-zoneid"))
	{
		zoneID = RP_Get_Integer("tp-zoneid");
	}
	else
	{
		Message(
"\tHOST: WARNING! You must set the rp-variable \"tp-zoneid\" using the following console command:\n\n\
\t\t (rp-var-define 'tp-zoneid ZONE_ID 'integer #f),\n\n\
\twhere ZONE_ID equals the sampling plane cell zone id (see Cell Zones in the structure tree).\n"
		);
		zoneID = -1;
	}
	filePrefix = RP_Variable_Exists_P("tp-prefix") ? RP_Get_String("tp-prefix") : "TP";
	filePrecision = RP_Variable_Exists_P("tp-precision") ? RP_Get_Integer("tp-precision") : 6;
	
	PRF_CSEND_INT(node_zero, &zoneID, 1, node_host);
	
	Message("\tHOST: tpWriteInit complete.\n\n");
#endif

#if	!RP_HOST
	if (I_AM_NODE_ZERO_P)
	{
		PRF_CRECV_INT(node_host, &zoneID, 1, node_host);
		compute_node_loop_not_zero(pe)
		{
			PRF_CSEND_INT(pe, &zoneID, 1, node_zero);
		}
	}
	else
	{
		PRF_CRECV_INT(node_zero, &zoneID, 1, node_zero);
	}
#endif
}

DEFINE_EXECUTE_AT_END(tpWriteAtEnd)
{	
	double	data[size];
	int		amDone[2], i, nodesDone = 0, pe, pf;
	int		isNodeDone[compute_node_count];
	int		j = coordsWritten ? 1 : 2;
	
#if !RP_NODE
	char	proFileName[2][buffer] = {"", ""};
	char	fpBuffer[buffer];
	char	flowTimeC[buffer];
	FILE	*proFile[2];
#endif

#if !RP_HOST
	cell_t	c;
	Domain	*d;
	float	x[3];	
	Thread	*t;
	
	amDone[0] = myid;
	amDone[1] = 0;
#endif

	for (i = 0; i < compute_node_count; i++)
	{
		isNodeDone[i] = 0;
	}
	
#if !RP_NODE
	sprintf(fpBuffer, "%%.%df", filePrecision);
	sprintf(flowTimeC, fpBuffer, RP_Get_Real("flow-time"));
	sprintf(proFileName[0], "./tp_pre/%s_U_%s.csv", filePrefix, flowTimeC);
	sprintf(proFileName[1], "./tp_pre/%s_C.csv", filePrefix);
	
	for (i = 0; i < j; i++)
	{
		proFile[i] = fopen(proFileName[i], "w");
		if (proFile[i] == NULL)
		{	
			Message("\tHOST: Can't open file :(\n");
			exit(-1);
		}
	}
	Message("\tHOST: Writing profile for t = %s s... ", flowTimeC);
#endif	
	
#if !RP_HOST
	d = Get_Domain(1);	
	t = Lookup_Thread(d, zoneID);
	pe = (I_AM_NODE_ZERO_P) ? node_host : node_zero;
	
	if (NNULLP(t))
	{
		begin_c_loop(c, t)
		{
			amDone[1] = 0;

			C_CENTROID(x,c,t);
			
			data[0] = C_U(c,t);
			data[1] = C_V(c,t);
			data[2] = C_W(c,t);	
			data[3] = x[0];
			data[4] = x[1];
			data[5] = x[2];
			
			PRF_CSEND_INT(pe, amDone, 2, myid);
			PRF_CSEND_DOUBLE(pe, data, size, myid);	
		}
		end_c_loop(c, t)
	}
	
	if (I_AM_NODE_ZERO_P)
	{
		pf = 1;
		while (nodesDone < compute_node_count - 1) 
		{
			nodesDone = 0;
			pf = pf % compute_node_count;
			pf = (pf == 0) ? 1 : pf;
			if (isNodeDone[pf] == 0)
			{
				PRF_CRECV_INT(pf, amDone, 2, pf);
				PRF_CSEND_INT(node_host, amDone, 2, node_zero);
				isNodeDone[amDone[0]] = amDone[1];
				if (isNodeDone[amDone[0]] == 0)
				{
					PRF_CRECV_DOUBLE(pf, data, size, pf);
					PRF_CSEND_DOUBLE(node_host, data, size, node_zero);
				}
			}
			for (i = 0; i < compute_node_count; i++)
			{
				nodesDone += isNodeDone[i];
			}
			pf++;
		}
		amDone[0] = node_zero;
	}
	
	amDone[1] = 1;
	PRF_CSEND_INT(pe, amDone, 2, myid);
#endif

#if !RP_NODE
	while (nodesDone < compute_node_count) 
	{
		nodesDone = 0;
		PRF_CRECV_INT(node_zero, amDone, 2, node_zero);
		isNodeDone[amDone[0]] = amDone[1];
		if (isNodeDone[amDone[0]] == 0)
		{
			PRF_CRECV_DOUBLE(node_zero, data, size, node_zero);
			for (i = 0; i < j; i++)
			{
				fprintf(proFile[i], "%.6f,%.6f,%.6f\n", data[3*i], data[3*i+1], data[3*i+2]);	
			}
		}
		for (i = 0; i < compute_node_count; i++)
		{
			nodesDone += isNodeDone[i];
		}
	}
	Message("done.\n");	
	for (i = 0; i < j; i++)
	{
		fclose(proFile[i]);
	}
	coordsWritten = true;
#endif
}