import time
import requests

url = 'http://URL:10000/depth_to_xyz'

img = None

payload = open('0_depth.png', 'rb')
response = requests.post(url, data=payload)
img = response.content

