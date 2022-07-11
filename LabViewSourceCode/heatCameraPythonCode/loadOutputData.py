
filename = "C:\\Users\\labadmin\\Documents\\projectContributors\\Michael\\multi-larva-cl-behavior-detection\\LabViewSourceCode\\heatCameraPythonCode\\"
name = "recording3.bin"
filename += name
# file = open(filename, "rb")

import numpy as np
import matplotlib.pylab as plt

f = open(filename, "rb")
a = np.fromfile(f, dtype=np.float64)
#a = a - np.min(a)
#a = a / np.max(a)
#b = 27 + (a*11)

plt.plot(a)
plt.plot(plt.gca().get_xlim(), [37,37], '--', color="black")
plt.ylabel("Temperature (C)")
plt.show()