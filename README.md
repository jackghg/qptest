# qptest
QPTest is a standalone app to test LTE quectel EC2x modules(and maybe some other qualcomm modem), like in the pinephone. C++ and gtk2. You can test calls, sms, gps, network status, internet.  
## Install
Require alsa lib, alsaloop, qmicli, busybox udhcpc.  
On arch:  
`sudo pacman -S alsa-lib alsa-utils libqmi busybox`  
On Debian, ubuntu:  
`sudo apt install libgtk-2-dev alsa-utils libasound2 libasound-dev libqmi-utils busybox`
- Get files  
`git clone https://github.com/jackghg/qptest.git`
- Insert your mobile operator apn in the /qmi/qmic script
- correct the variables in the 'variables to customize' section, on top of the source code; mostly devices name.
- You may need to update the 'simple.script' file from busybox sources, /examples/udhcp/simple.script  
- Compile:  
```g++ qptest.c -o qptest `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` -lasound```  
- The install script will copy some files but not qptest executable    
`sudo ./install`  
- Run as root:  
`sudo ./qptest`
- On the first time you run qptest, click the 'First Run Setup' button and restart the modem/device.
