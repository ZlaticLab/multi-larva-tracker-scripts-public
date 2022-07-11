import numpy as np
from skimage.transform import ProjectiveTransform
# C:\Users\labadmin>C:\Users\labadmin\AppData\Local\Programs\Python\Python39-32\python.exe

# State larva system coordinates
# xmin, xmax = 0,1
# ymin, ymax = 0,1
# sysCoords = np.array([[xmin,ymin],[xmin,ymax],[xmax,ymax],[xmax,ymin]])
topLeft = [192,128]
topRight = [3027,115]
bottomLeft = [204,2937]
bottomRight = [3020,2937]
sysCoords = np.array([bottomLeft, topLeft, topRight, bottomRight])

# Convert readings to location
ImageHeight = 3072; PixelResolution = PixelResolution=75.84
x, y = sysCoords[:,0], sysCoords[:,1]
xLoc = np.array(np.round(x/1000 * PixelResolution), dtype=int)
yLoc = np.array(PixelResolution*(ImageHeight-y)/1000, dtype=int)
sysCoords = np.array([xLoc, yLoc]).T

# State corresponding heat camera system coordinates
topLeft = [191,106]
topRight = [498,106]
bottomLeft = [176,403]
bottomRight = [509,403]
heatCoords = np.array([bottomLeft, topLeft, topRight, bottomRight])

# Estimate projection between larva system and heat camera
t = ProjectiveTransform()
t.estimate(sysCoords, heatCoords)

# Get homogeneous transformation matrix
transformationMatrix = t.params

# Get larva system samples
sampleSquare = lambda coords : [np.random.uniform(np.min(coords[:,i]), np.max(coords[:,i])) for i in range(2)]
squareSamples = np.array([sampleSquare(sysCoords) for i in range(500)])

# Get transformed samples
transformedSamples = t(squareSamples)

# Plot
import matplotlib.pylab as plt
fig,ax = plt.subplots(1,2)
transCoords = t(sysCoords)
ax[0].scatter(squareSamples[:,0],squareSamples[:,1])
ax[1].scatter(transformedSamples[:,0],transformedSamples[:,1])
# for i in range(len(transformedSamples)):
#     ax[0].scatter(squareSamples[i,0],squareSamples[i,1])
#     ax[1].scatter(transformedSamples[i,0],transformedSamples[i,1])
plt.show()


# Calculation
def performTransformation(x,y,transMatrix):
    coords = [x,y]
    coords = np.array(coords, copy=False, ndmin=2)
    x, y = np.transpose(coords)
    src = np.vstack((x, y, np.ones_like(x)))
    dst = src.T @ transMatrix.T
    # below, we will divide by the last dimension of the homogeneous
    # coordinate matrix. In order to avoid division by zero,
    # we replace exact zeros in this column with a very small number.
    dst[dst[:, 2] == 0, 2] = np.finfo(float).eps
    # rescale to homogeneous coordinates
    dst[:, :2] /= dst[:, 2:3]
    return dst[:, :2]

def test(x, y, transMatrix):
    return x,y

x = 100; y = 100
print(performTransformation(x,y, transformationMatrix))
np.savetxt("transformationMatrix.csv", transformationMatrix)