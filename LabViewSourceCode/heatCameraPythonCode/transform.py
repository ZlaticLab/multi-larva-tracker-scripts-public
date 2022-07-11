import numpy as np

# Calculation
def performTransformation(x,y,transMatrix):
    coords = [x,y]
    transMatrix = np.array(transMatrix)
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
    dst = dst[0]
    return [dst[0], dst[1]]


def trans(x,y,transMatrix):
    coords = [x,y]
    transMatrix = np.array(transMatrix)
    log = ""
    log += str(transMatrix)
    log += str(coords)
    text_file = open("C:\\Users\\labadmin\\Documents\\projectContributors\\Michael\\multi-larva-cl-behavior-detection\\LabViewSourceCode\\heatCameraPythonCode\\Output.txt", "w")
    text_file.write(log)
    text_file.close()
    return [1,2]