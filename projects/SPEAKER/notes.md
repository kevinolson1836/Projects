# Custom Speaker


## The idea

-  I wanted to make speakers for my desk. I wanted them to be unique and diffrent from the average speaker, the quality did not really matter to me, as long as it sounded decent. In the past I built audio reactive lights, I grabed the raw audio signal and used Fast Fourier Transform (FFT) to make fancy animations! So I tried to combine them! a speaker with a transpernt front with a ring of LEDS that dance!


## Intro

- As a good amount of my projects here, This is writen after I "finished" this project. so this is more of a post dev log


## The inspiration

- As I said in the intro, from what I remebered, I wanted speakers for my desk. Before starting the designing, I new I wanted 2 speakers that have a unique look to them. After some back and forth I settled on a egg shape. I liked the way this looked and it had a flat surface for it the lay on and it was a shape I can easily 3D print!

- I knew I wanted it to be 3D printed and with my FDM printer I knew I was going to have some bad layer lines on the inside of the speaker throwing off some of the quality of the sound, but quaility was not a priority for me. Because I was using my printer and I had some translucaint filament on hand, I made the front out of that so it was able to show off the lights.

## Hardware

- I dont remeber exactly what hardware I used but given my history this is close to what I would have used per speaker. you will also need a soldering iron, 3D Printer and wire.

    - Teensy
    - ws2812b LED's
    - Bluetooth audio receiver
    - cheap and small amp
    - cheap $5 speaker


## Software

- I was using the Teensy as the brains for the effects. I noramly use a ESP32 as the brains but for this project, i needed the extra horse power that the Teensy has. I needed the hardware support for the speed, This had to be as fast as possible so the lights would sync up correctly with the audio. I split the audio signal with out any delay directly into the speaker and into the Teensy.

- I used FFT's to grab the patterns out of the audio signals and made some sort of "heart beat" out of it for the light to dance along with.