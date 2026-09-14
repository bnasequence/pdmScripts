import glob
import numpy as np
import os
import re
from scipy.spatial import KDTree
import string
import sys

inletName = sys.argv[1]
profileInDir = sys.argv[2]
profileOutDir = sys.argv[3]
initTimestep = float(sys.argv[4]) if (len(sys.argv) > 4) else 0    

inletFile = np.genfromtxt(inletName, delimiter = ",", autostrip = True)
crdIn = inletFile[:,2:5]
datIn = inletFile[:,0:2]

crdPrFile = glob.glob(profileInDir + '/*_C.csv')[0]
crdPr = np.genfromtxt(crdPrFile, delimiter = ",", autostrip = True)

treePr = KDTree(crdPr)
dPr, iPr = treePr.query(crdIn[:], k = 1)

proFiles = glob.glob(profileInDir + '/*_U_*.csv')
getProfileName = lambda f: os.path.splitext(os.path.basename(f))[0]
getPrefix = lambda f: re.findall("\D+_", getProfileName(f))[0]
getTimestep = lambda f: re.findall("\d+\.\d+", getProfileName(f))[0]
getPrecision = len(getTimestep(proFiles[0]).split('.')[1])
timestepSize = float(getTimestep(proFiles[1])) - float(getTimestep(proFiles[0]))

for i in range(len(proFiles)):	
	datPr = np.genfromtxt(proFiles[i], delimiter = ",", autostrip = True)
	
	outFile = np.hstack((datIn[:], datPr[iPr]))	
	outFilename = (
		profileOutDir + "/" + getPrefix(proFiles[i]) + 
		f"{initTimestep + i * timestepSize:.{getPrecision}f}" + ".csv"
		)
	np.savetxt(outFilename, outFile, fmt = "%d,%d,%.6f,%.6f,%.6f")
	
    with open(outFilename, 'rb+') as fout:
		fout.seek(-2, 2)
		fout.truncate()