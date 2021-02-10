# qptest
Test LTE quectel EC2x modules (and maybe some other qualcomm modem), like in the pinephone. Standalone, made in c++ and gtk2. You can test calls, sms, gps, network status, internet.  
Require alsa lib, alsaloop, qmicli, busybox udhcpc.  
Customize variables in source code and scripts. You may want the latest verion of the simple.script from busybox sources.  
Compile:  
```g++ qptest.c -o qptest `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` -lasound```  
Install files:  
`sudo ./install`  
Run as root:  
`sudo ./qptest`
