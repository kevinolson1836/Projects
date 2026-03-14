import requests
import time


while True:
    response = requests.get('http://10.0.0.36/on')
    print(response.text)
    time.sleep(5)
    response = requests.get('http://10.0.0.36/off')
    print(response.text)
    time.sleep(5)