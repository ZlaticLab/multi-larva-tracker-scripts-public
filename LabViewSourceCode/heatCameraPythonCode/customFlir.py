import requests
import numpy as np
import urllib.request
import io
import binascii
from PIL import Image

session = requests.Session()
baseURL = "http://169.254.253.219/"
message = session.post(baseURL + 'login/dologin', data={'user_name':'admin','user_password':'admin'})

def getResource(resource):
    return session.post(baseURL + 'res.php', data={'action':'get','resource':resource})

def setResource(resource,value):
        message = session.post(baseURL + 'res.php', data={'action':'set','resource':resource,'value':value})

def CtoK(temp):
    return temp+273.15

setResource('.image.sysimage.palette.readFile',"bw.pal") # set palette to gray

def setTemperatureRange(minTemp, maxTemp):
    setResource('.image.contadj.adjMode', 'manual')
    setResource('.image.sysimg.basicImgData.extraInfo.lowT',CtoK(minTemp))
    setResource('.image.sysimg.basicImgData.extraInfo.highT',CtoK(maxTemp))

def getTemperatureValue(x, y, i):
    setResource('.image.sysimg.measureFuncs.spot.' + str(i) + '.active','true')
    setResource('.image.sysimg.measureFuncs.spot.' + str(i) + '.x',x)
    setResource('.image.sysimg.measureFuncs.spot.' + str(i) + '.y',y)
    temperature = getResource('.image.sysimg.measureFuncs.spot.' + str(i) + '.valueT').content
    temperature = temperature.decode("utf-8")
    return float(temperature[1:-2])

## setResource('.resmon.config.hideGraphics','false')

def getImage():
    data = urllib.request.urlopen('http://169.254.253.219/snapshot.jpg?user=admin&pwd=admin&strm=05').read()
    stream = io.BytesIO(data)
    image = Image.open(stream)
    img = np.array(image)
    stream.close()
    image.close()
    image = list(img[:,:,0].flatten())
    return image