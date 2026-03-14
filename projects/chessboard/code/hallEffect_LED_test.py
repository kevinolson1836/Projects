import wiringpi as wiringpi  
import RPi.GPIO as GPIO
from rpi_ws281x import PixelStrip, Color
import time

GPIO.setmode(GPIO.BCM)

hallEffectPin = 4

# LED strip configuration:
LED_COUNT = 256      # Number of LED pixels.
LED_PIN = 18          # GPIO pin connected to the pixels (18 uses PWM!).
# LED_PIN = 10        # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10          # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 50  # Set to 0 for darkest and 255 for brightest
LED_INVERT = False    # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53


 # Create NeoPixel object with appropriate configuration.
strip = PixelStrip(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
# Intialize the library (must be called once before other functions).
strip.begin()


  
pin_base = 65       # lowest available starting number is 65  
i2c_addr = 0x20     # A0, A1, A2 pins all wired to GND  
  
wiringpi.wiringPiSetup()                    # initialise wiringpi  
wiringpi.mcp23017Setup(pin_base,i2c_addr)   # set up the pins and i2c address  
  
wiringpi.pinMode(pin_base, 0)         # sets GPA0 to output  
wiringpi.pinMode(pin_base+1, 0)         # sets GPA0 to output  
wiringpi.pinMode(pin_base+2, 0)         # sets GPA0 to output  
wiringpi.pinMode(pin_base+3, 0)         # sets GPA0 to output  
# wiringpi.digitalWrite(65, 0)    # sets GPA0 to 0 (0V, off)  


def readpin(pin):
    p = wiringpi.digitalRead(pin)
    return(p)
    
def turnOnLeds(ID):
    ID = ID *4
    
    # EACH BOARD HAS 4 LEDS, TURN ON THE 4 LEDS FOR THAT BOARD ID
    strip.setPixelColor(ID  , Color(0, LED_BRIGHTNESS, 0))
    strip.setPixelColor(ID+1, Color(0, LED_BRIGHTNESS, 0))
    strip.setPixelColor(ID+2, Color(0, LED_BRIGHTNESS, 0))
    strip.setPixelColor(ID+3, Color(0, LED_BRIGHTNESS, 0))
   
    strip.show()




def turnOffLeds(ID):
    ID = ID *4
    
    # EACH BOARD HAS 4 LEDS, UPDATES THE 4 LEDS FOR THAT BOARD ID
    strip.setPixelColor(ID  , Color(LED_BRIGHTNESS, 0, 0))
    strip.setPixelColor(ID+1, Color(LED_BRIGHTNESS, 0, 0))
    strip.setPixelColor(ID+2, Color(LED_BRIGHTNESS, 0, 0))
    strip.setPixelColor(ID+3, Color(LED_BRIGHTNESS, 0, 0))
   
    strip.show()
    
time.sleep(2)             

             
turnOnLeds(0)
time.sleep(0.3)             
turnOnLeds(1) 
time.sleep(0.3)                         
turnOnLeds(2)             
time.sleep(0.3)             


old_state0 = 0
old_state1 = 0
old_state2 = 0
             
state0 = -1
state1 = -1
state2 = -1
             
while True:
    old_state0 = state0
    old_state1 = state1
    old_state2 = state2
    
    # read new state of pin
    state0 = wiringpi.digitalRead(pin_base)
    state1 = wiringpi.digitalRead(pin_base+1)
    state2 = wiringpi.digitalRead(pin_base+2)
    
    # if change in state turn on or off
    if (state0 != old_state0):
        if (state0 == 1):
            turnOnLeds(0)
            print("turn on 0 ")
        else:
            turnOffLeds(0)
            print("turn off 0")
     
    # if change in state turn on or off
    if (state1 != old_state1):
        if (state1 == 1):
            turnOnLeds(1)
            print("turn on 1")
        else:
            turnOffLeds(1)
            print("turn off 1")
            
    # if change in state turn on or off
    if (state2 != old_state2):
        if (state2 == 1):
            turnOnLeds(2)
            print("turn on 2")
        else:
            turnOffLeds(2)
            print("turn off 2")
            



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
            