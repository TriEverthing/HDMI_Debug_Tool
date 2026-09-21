# HDMI Debug Tool

&emsp;&emsp;这是一个基于CHV305FB的电视机串口/I2C调试工具。其核心原理模拟FTDI Vendor设备实现的，大部vendor代码基于libusb ftdi的Vendor请求实现。考虑一些涉密问题，工具只开放了BootLoader的源码，真正的应用程序只能提供编译好的固件。工具烧录好BootLoader后，只要按住按键上电，就可以uf2进行拖拽烧录，对后期固件更新很是友好。

![HDMI_Debug_Tool_PCBA](./Images/HDMI_Debug_Tool_PCBA.jpg)

&emsp;&emsp;工具支持公板/TCL/长虹/小米/海尔的串口定义，通过Button切换定义。板载电平转换芯片，可以灵活配置输出电平电压。兼容UART和I2C，可以适用Mstar官方调试工具。

# 测试
## 串口功能测试

&emsp;&emsp;目前我只能测试小米电视，将小板连接到小米电视支持HDMI打印的串口。先按下Button，将功能切换小米电视串口。

![小米电视串口打印](./Images/XiaoMiTV_MobaXterm.png)

## I2C功能测试

&emsp;&emsp;使用Mstar调试测试I2C功能，小板和主板均能正常识别。使用ISP工具烧录固件到TV Board，固件完整烧录，主板正常启动。

![alt text](./Images/Msart_Tool.png)