import wiringpi as wiringpi  
from time import sleep  
import RPi.GPIO as GPIO
from rpi_ws281x import PixelStrip, Color

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
  
pin_base = 66       # lowest available starting number is 65  
i2c_addr = 0x20     # A0, A1, A2 pins all wired to GND  
  
wiringpi.wiringPiSetup()                    # initialise wiringpi  
wiringpi.mcp23017Setup(pin_base,i2c_addr)   # set up the pins and i2c address  
  
wiringpi.pinMode(pin_base, 0)         # sets GPA0 to output  
# wiringpi.digitalWrite(65, 0)    # sets GPA0 to 0 (0V, off)  
  

def turnOnLeds():
   
    for i in range(LED_COUNT):
        strip.setPixelColor(i, Color(0, 255, 0))
   
    strip.show()
    print("on")



def turnOffLeds():
    for i in range(LED_COUNT):
        strip.setPixelColor(i, Color(255, 0, 0))
   
    strip.show()
    print("off")
  
def didStateChange(pin, prev):
      p = wiringpi.digitalRead(pin)
      return(p)
    #   print(str(prev) + "   " + str(p))
    #   print ("pin == prev : ", end = '')
    #   print(p == prev)
      
      if(p == prev):
        #   print("return 0")
          return(1)
      else:
          return(0)
prev = -1

def custom_sleep(times):
    for x in range(times * 9999999):
        pass 
    strip.show()

while True:
	reading = wiringpi.digitalRead(pin_base)
	if (reading):
		turnOnLeds()
	else:
		turnOffLeds()
    #temp = didStateChange(pin_base, prev)
    #prev = temp
    # print (temp)
    # if (temp):
    #custom_sleep(2)
    #turnOffLeds()
    #sleep(1)
    # custom_sleep(2)
    
    
# else:
    #turnOnLeds()
    #sleep(1)
    # strip.show()
        
  
# wiringpi.pinMode(80, 0)         # sets GPB7 to input  
# wiringpi.pullUpDnControl(80, 2) # set internal pull-up   
  
# # Note: MCP23017 has no internal pull-down, so I used pull-up and inverted  
# # the button reading logic with a "not"  
  
# try:  
#     while True:  
#         if not wiringpi.digitalRead(80): # inverted the logic as using pull-up  
#             wiringpi.digitalWrite(65, 1) # sets port GPA1 to 1 (3V3, on)  
#         else:  
#             wiringpi.digitalWrite(65, 0) # sets port GPA1 to 0 (0V, off)  
#         sleep(0.05)  
# finally:  
#     wiringpi.digitalWrite(65, 0) # sets port GPA1 to 0 (0V, off)  
#     wiringpi.pinMode(65, 0)      # sets GPIO GPA1 back to input Mode  
#     # GPB7 is already an input, so no need to change anything  
