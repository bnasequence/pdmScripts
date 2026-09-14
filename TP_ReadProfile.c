#include "udf.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define	buffer		1024
#define	dataLength	5

bool		dataArrayAllocated = false;
const char	*filePrefix;
double		**dataArray;
int			dataArraySize = buffer * 10;
int			filePrecision;

DEFINE_EXECUTE_ON_LOADING(tpReadOnLoading, libname)
{
#if !RP_NODE
	Message(
"\n\tHOST: 1) Since DEFINE_VELOCITY macros are executed the moment you load the dll,\n\
\tthe X, Y and Z Velocity components will first be initialised to 0 values.\n\
\tHOST: 2) If your profile prefix and timestep precision differ from the default values\n\
\tchange them by defining the \"tp-prefix\" and \"tp-precision\" rp-variables:\n\n\
\t\t (rp-var-define 'tp-prefix PREFIX 'string #f),\n\
\t\t (rp-var-define 'tp-precision PRECISION 'integer #f).\n\n\
\tHOST: 3) Run \"tpReadInit\" UDF by going to User-Defined > Execute on Demand\n\
\tin order to initialise the variables used by the tpRead scripts and load the\n\
\tfirst transient velocity profile .csv-file.\n\
\tHOST: 4) Go to your inlet in the structure tree, choose Components for Velocity Specification Method\n\
\tand select \"udf UX::libname\", \"udf UX::libname\" and \"udf UX::libname\" for X-Velocity, Y-Velocity and Z-Velocity,\n\
\trespectively.\n\
\tHOST: 5) Finally, hook \"tpReadAtEnd\" UDF by going to User-Defined > Function Hooks... > Execute at End.\n\n"
	);
#endif	
}

void readProfile()
{	
	int 	i, j, k, pe;
#if !RP_NODE
	cell_t	c;
	char	proFileName[buffer];
	char	fpBuffer[buffer];
	char	flowTimeC[buffer];
	char	row[buffer], *token;
	double	**dataArrayOld;
	FILE	*proFile;	
	int		dataArrayOldSize = 0;

	sprintf(fpBuffer, "%%.%df", filePrecision);
	sprintf(flowTimeC, fpBuffer, RP_Get_Real("flow-time"));
	sprintf(proFileName, "./tp_sim/%s_U_%s.csv", filePrefix, flowTimeC);
	
	proFile = fopen(proFileName, "r");
	if(proFile != NULL)
	{	
		Message("\n\tHOST: Reading profile for t = %s s... ", flowTimeC);
		if(!dataArrayAllocated)
		{
			dataArray = (double **)malloc(dataArraySize * sizeof(double *));
			for (j = 0; j < dataArraySize; j++)
			{
				dataArray[j] = (double *)malloc(dataLength * sizeof(double));
			}
		}	
		i = 0;
		while (feof(proFile) != true)
		{
			if (!dataArrayAllocated)
			{
				if(i == dataArraySize)
				{
					dataArrayOldSize = dataArraySize;
					dataArraySize *= 2;
					dataArray = (double **)realloc(dataArray, dataArraySize * sizeof(double *));
					for (j = dataArrayOldSize; j < dataArraySize; j++)
					{
						dataArray[j] = (double *)malloc(dataLength * sizeof(double));
					}
				}
			}
			fgets(row, buffer, proFile);
			token = strtok(row, ",");
			for (j = 0; j < dataLength; j++)
			{	
				sscanf(token, "%lf", &dataArray[i][j]);
				token = strtok(NULL, ",");
			}	
			i++;
		}	
		fclose(proFile);
		if(!dataArrayAllocated)
		{
			dataArraySize = i;
			dataArray = (double **)realloc(dataArray, dataArraySize * sizeof(double *));
			PRF_CSEND_INT(node_zero, &dataArraySize, 1, node_host);		
			dataArrayAllocated = true;
		}	
		Message("done. Sending data to nodes... ");
		for(i = 0; i < dataArraySize; i++)
		{
			PRF_CSEND_DOUBLE(node_zero, dataArray[i], dataLength, node_host);
		}
		Message("data sent.\n");
	}
	else
	{
		Message("\n\tHOST: Can't open profile :(\n\n");
		exit(-2);
	}
#endif
#if !RP_HOST
	pe = (I_AM_NODE_ZERO_P) ? node_host : node_zero;
	if (!dataArrayAllocated)
	{
		PRF_CRECV_INT(pe, &dataArraySize, 1, pe);	
		dataArray = (double **)malloc(dataArraySize * sizeof(double *));
		for(i = 0; i < dataArraySize; i++)
		{
			dataArray[i] = (double *)malloc(dataLength * sizeof(double));
		}
	}
	for(i = 0; i < dataArraySize; i++)
	{
		PRF_CRECV_DOUBLE(pe, dataArray[i], dataLength, pe);
	}		
	if(I_AM_NODE_ZERO_P)
	{
		compute_node_loop_not_zero(pe)
		{
			if (!dataArrayAllocated)
			{	
				PRF_CSEND_INT(pe, &dataArraySize, 1, node_zero);
			}
			for(i = 0; i < dataArraySize; i++)
			{
				PRF_CSEND_DOUBLE(pe, dataArray[i], dataLength, node_zero);
			}
		}
	}
	dataArrayAllocated = true;
#endif
}

DEFINE_ON_DEMAND(tpReadInit)
{
#if !RP_NODE
	filePrefix = RP_Variable_Exists_P("tp-prefix") ? RP_Get_String("tp-prefix") : "TP";
	filePrecision = RP_Variable_Exists_P("tp-precision") ? RP_Get_Integer("tp-precision") : 6;
	Message("\tHOST: tpReadInit complete.\n\n");
#endif
	readProfile();
}

DEFINE_EXECUTE_AT_END(tpReadAtEnd)
{	
	readProfile();
}

DEFINE_PROFILE(UX, t, i)
{
	face_t	f;
	int		j;
	
	if(dataArraySize <= 0)
	{
		Message0("\tNODE 0: Transient profile not initialised, setting X-Velocity to 0...\n");
		begin_f_loop(f,t)
		{
			F_PROFILE(f,t,i) = 0.0;
		}
		end_f_loop(f,t)
	}
	else
	{
		for(j = 0; j < dataArraySize; j++)
		{
			if((int)dataArray[j][0] == myid)
			{
				begin_f_loop(f,t)
				{
					F_PROFILE(f,t,i) = dataArray[j + f][2];
				}
				end_f_loop(f,t)
				break;
			}
		}
	}
}

DEFINE_PROFILE(UY, t, i)
{
	face_t	f;
	int		j;
	
	if(dataArraySize <= 0)
	{
		Message0("\tNODE 0: Transient profile not initialised, setting Y-Velocity to 0...\n");
		begin_f_loop(f,t)
		{
			F_PROFILE(f,t,i) = 0.0;
		}
		end_f_loop(f,t)
	}
	else
	{
		for(j = 0; j < dataArraySize; j++)
		{
			if((int)dataArray[j][0] == myid)
			{
				begin_f_loop(f,t)
				{
					F_PROFILE(f,t,i) = dataArray[j + f][3];
				}
				end_f_loop(f,t)
				break;
			}
		}
	}
}

DEFINE_PROFILE(UZ, t, i)
{
	face_t	f;
	int		j;
	
	if(dataArraySize <= 0)
	{
		Message0("\tNODE 0: Transient profile not initialised, setting Z-Velocity to 0...\n");
		begin_f_loop(f,t)
		{
			F_PROFILE(f,t,i) = 0.0;
		}
		end_f_loop(f,t)
	}
	else
	{
		for(j = 0; j < dataArraySize; j++)
		{
			if((int)dataArray[j][0] == myid)
			{
				begin_f_loop(f,t)
				{
					F_PROFILE(f,t,i) = dataArray[j + f][4];
				}
				end_f_loop(f,t)
				break;
			}
		}
	}
}