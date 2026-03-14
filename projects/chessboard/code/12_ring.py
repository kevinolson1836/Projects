import RPi.GPIO as GPIO
from rpi_ws281x import PixelStrip, Color
import time


GPIO.setmode(GPIO.BCM)

hallEffectPin = 14

# LED strip configuration:
LED_COUNT = 12       # Number of LED pixels.
LED_PIN = 18          # GPIO pin connected to the pixels (18 uses PWM!).
# LED_PIN = 10        # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10          # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 100  # Set to 0 for darkest and 255 for brightest
LED_INVERT = False    # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

GPIO.setup(hallEffectPin, GPIO.IN)

 # Create NeoPixel object with appropriate configuration.
strip = PixelStrip(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
# Intialize the library (must be called once before other functions).
strip.begin()
    
    
def turnOnLeds():
   
    for i in range(LED_COUNT):
        strip.setPixelColor(i, Color(0, 255, 0))
   
    strip.show()



def turnOffLeds():
    for i in range(LED_COUNT):
        strip.setPixelColor(i, Color(255, 0, 0))
   
    strip.show()


current = 0
prev = 0

while True:
    current = GPIO.input(hallEffectPin)

    if current != prev:
        print(f"Pin {hallEffectPin} is HIGH")
        turnOffLeds()
    else:
        print(f"Pin {hallEffectPin} is LOW")
        turnOnLeds()
    prev = current
