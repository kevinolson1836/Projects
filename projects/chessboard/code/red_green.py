import RPi.GPIO as GPIO
from rpi_ws281x import PixelStrip, Color
import time


GPIO.setmode(GPIO.BCM)

hallEffectPin = 4

# LED strip configuration:
LED_COUNT = 12      # Number of LED pixels.
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
    
    
def turnOnLeds(ID):
    ID = 4
    for ID in range(50):
        strip.setPixelColor(ID  , Color(0, 255, 0))
   
    strip.show()




def turnOffLeds(ID):
    # ID = 4
    for ID in range(50):
        strip.setPixelColor(ID, Color(255, 0, 0))

    strip.show()

        
old_state = 0
state = 0
             
while True:
    turnOffLeds(0)
    print("off")
    
    time.sleep(2)
    
    turnOnLeds(0)
    print("on")  
    
    time.sleep(2)






# TODO:
#     MAKE A CLASS FOR A SQUARE
#         HOLDS OLD STATE 
#         HOLDS A ID FOR QUARE NUMBER
#         HOLD A ID FOR PIN ON GPIO EXTNDER
#         HAS A FUNCTION TO SET COLOR 
#         HAS A FUNCTION TO CLEAR AND RESET
#   
#      GAME ENGINE 
#         CHESS ENGINE WILL DO MOST OF THE HEAVY LIFTING
#   
#      FUNCTIONS TO LIGHT UP EACH SQUARE BASED ON WHAT MOVES IT CAN MAKE
            