# HDMI Debug Tool



这是一个基于CHV305FB的电视机串口/I2C调试工具。其核心原理模拟FTDI Vendor设备实现的，大部vendor代码基于libusb ftdi的Vendor请求实现。考虑一些涉密问题，工具只开放了BootLoader的源码，真正的应用程序只能提供编译好的固件。工具烧录好BootLoader后，只要按住按键上电，就可以uf2进行拖拽烧录，对后期固件更新很是友好。

