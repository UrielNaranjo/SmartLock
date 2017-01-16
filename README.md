# SmartLock
	Smart Lock is a bluetooth controlled electromagnetic lock that is designed to improve
door lock security. It allows you to no longer have to carry your keys around as you can
unlock or lock your door using your smartphone. Smart Lock is equipped with a multi-step
lock function that allows you to setup three different passwords to further secure your 
door. If someone attempts to guess these passwords too many times, they will trigger the 
alarm which will then disable any further attempts. This will prevent anyone from entering
for an extended period of time.

Short Demo Below:

<a href="https://youtu.be/E6MVggHiowo
" target="_blank"><img src="http://imgur.com/6XV9gjy" 
alt="Timed brightness option in Settings (In Development)" width="240" height="180" border="5" /></a>

## Technology

* AVRStudio 6.0
* ATmega1284
* ElectroMagnetic Lock
* SmartPhone
* LightBlue iOS app
* HM-10 Bluetooth Low Energy Module 
* LCD Screen
* RGB LED Matrix
* Speaker

## Functionality
* Electromagnetic Lock: This is used to lock the door using an electromagnet that is capable
of holding up to 130 lbs. It works by inducing a magnetic field when a current is passed to it.
To lock it, pass current and to unlock it do not pass current.

* Bluetooth Communication: The HM-10 Bluetooth module uses USART to communicate data from a
smart phone to the micro-controller. The passwords are input from the smart phone and the
microcontroller processes the data and decides what to do with it.

* LCD Screen: The LCD screen receives basic information from the master microcontroller through
USART and displays it on the screen. This includes the status of the door whether locked, 
unlocked, alarm, or whether the user is resetting the password. In addition, it tells you 
the current lock you are on, the number of attempts left to the user, and the lock you are 
currently resetting if you are resetting. Masking is used in order to extract the different 
displayed data received from the USART.

* RGB LED Matrix:The LED Matrix provides an image view of what is going on with the lock. It 
will display a different image depending on the status and/or circumstances of the door lock.

* Speaker: The speaker provides audio feedback whenever data is sent to the microcontroller
from the smart phone. It is also used to sound a loud noise whenever the alarm is triggered.
