
DTS配置：

/* 注意下面关于UART的配置：uart4_xfer/uart4_rts/uart4_ctsn
 * 每个平台的名字可能不一样，要在对应芯片平台的dts/dtsi里面找下对
 * 应的uart写法，比如uart4_ctsn有些平台的名字为uart4_cts.
 */
wireless-bluetooth {
    compatible = "bluetooth-platdata"
    /* 这里要配置对应主控UART的RTS脚 */
    uart_rts_gpios = <&gpio4 RK_PA7 GPIO_ACTIVE_LOW>;
    pinctrl-names = "default", "rts_gpio";
    pinctrl-0 = <&uart4_rts>;
    pinctrl-1 = <&uart4_rts_gpio>;
    /* BT_REG_ON 蓝牙电源的开关 */
    BT,power_gpio = <&gpio4 RK_PB3 GPIO_ACTIVE_HIGH>;
    status = "okay";
};

/* 打开对应的UART配置 */
&uart4 {
    pinctrl-names = "default";
    /* 这里配置对应主控UART的TX/RX/CTS PIN ，不要配置RTS PIN*/
    pinctrl-0 = <&uart4_xfer &uart4_ctsn>;
    status = "okay";
};

/* uart4_rts_gpio */
&pinctrl {
    wireless-bluetooth {
        uart4_rts_gpio: uart4-rts-gpio {
            rockchip,pins = <4 RK_PA7 RK_FUNC_GPIO &pcfg_pull_none>;
        };
    };
};

内核defconfig增加如下配置：
CONFIG_BT=y
CONFIG_BT_BREDR=y
CONFIG_BT_RFCOMM=y
CONFIG_BT_LE=y
CONFIG_BT_DEBUGFS=y
CONFIG_RFKILL=y
CONFIG_RFKILL_RK=y


RTK蓝牙工具说明：
下面以RTL8822CS为例，首先找模组厂获取对应模块的驱动包，例如BT：20201202_LINUX_BT_DRIVER_RTL8822C_COEX_v1c1c

包含
fw/config文件，蓝牙初始化时会用到：
/lib/firmware/rtlbt/rtl8822cs_fw
/lib/firmware/rtlbt/rtl8822cs_config

蓝牙初始化程序：通过对应SDK的工具链去编译
rtk_hciattach$ tree
.
├── hciattach.c
├── hciattach.h
├── hciattach_h4.c
├── hciattach_h4.h
├── hciattach_rtk.c
├── Makefile
├── rtb_fwc.c
├── rtb_fwc.h
└── rtk_hciattach

#特别注意：fw/config文件的存放位置，根据不同SDK的实际情况去修改位置
rtb_fwc.c
39:#define FIRMWARE_DIRECTORY   "/lib/firmware/rtlbt/"
40:#define BT_CONFIG_DIRECTORY  "/lib/firmware/rtlbt/"

蓝牙UART接口驱动：使用对应SDK的内核编译工具链去编译出hci_uart.ko
bluetooth_uart_driver/
├── hci_h4.c
├── hci_ldisc.c
├── hci_rtk_h5.c
├── hci_uart.h
├── Kconfig
├── Makefile
├── rtk_coex.c
└── rtk_coex.h


初始化说明: (务必确保上面的DTS/defoconfig配置正确并生效)
# UART 接口：
killall rtk_hciattach 							#首先必须确保先关掉此进程(如果之前有打开)
echo 0 > /sys/class/rfkill/rfkill0/state 		#下电
sleep 0.5
echo 1 > /sys/class/rfkill/rfkill0/state 		#上电
sleep 0.5
insmod /usr/lib/modules/hci_uart.ko 			# realtek模组需要加载uart驱动
rtk_hciattach -n -s 115200 /dev/ttyS4 rtk_h5 & 	# ttySX指的是蓝牙使用哪个uart口

#正常的话可以看到hci0节点
hciconfig -a

应用开发：
BLE_APP/
btgatt-server.c

#编译方法：（arm-rockchip830-linux-uclibcgnueabihf-gcc RV1106交叉编译工具链，可根据实际情况替换）
arm-rockchip830-linux-uclibcgnueabihf-gcc -static -o btgatt_server btgatt-server.c uuid-helper.c libbluetooth-internal.a  libshared-mainloop.a -I .

### 做SERVER：一般是iot设备提供比如电量/心跳等被动监听的设备，且也是发起广播的设备；
btgatt-server.c

GATT服务参考：示例实现了Heart Rate Service
static void populate_hr_service(struct server *server)

#设置广播
##LE Set Random Address Command
hcitool -i hci0 cmd 0x08 0x0005 05 1b c1 72 52 5a

##LE Set Advertising Parameters Command
hcitool -i hci0 cmd 0x08 0x0006 A0 00 A0 00 00 01 00 00 00 00 00 00 00 07 00

##LE Set Advertising Data Command
hcitool -i hci0 cmd 0x08 0x0008 1b 02 01 02 03 03 10 19 13 09 52 4f 43 4b 43 48 49 50 5f 41 55 44 49 4f 5f 42 4c 45

##LE Set Scan Response Data Command
hcitool -i hci0 cmd 0x08 0x0009 17 16 ff 46 00 02 1c 02 04 54 01 00 00 08 54 00 00 00 00 00 00 36 01 00

##LE Set Advertise Enable Command
hcitool -i hci0 cmd 0x08 0x000a 1

#启动服务：
# btgatt_server -t public -r

rkbt_gatt_client:
echo -e -n "\xFF\xFF\x02\x13\x01" > /dev/ttyS5  测试配网按键
/oem/usr/bin/rk_hciattach -n -s 115200 -m 1  /dev/ttyS5 rockchip 3000000 flow nosleep 55:66:77:88:99:bb &
./data/rkbt_gatt_client -d 55:66:77:88:99:aa -m 100
register-notify 0xd
write-value -w 0xd 0x77 0x69 0x66 0x69 0x6c 0x69 0x73 0x74 0x73
write-value -w 0xd 0x77 0x69 0x66 0x69 0x5f 0x73 0x65 0x74 0x75 0x70 0x20 0x48 0x33 0x43 0x5f 0x36 0x35 0x35 0x42 0x33 0x41 0x20 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38

rkbt_gatt_server:
echo -e -n "\xFF\xFF\x02\x13\x01" > /dev/ttyS5  测试配网按键
./data/rkbt_gatt_server
uart_test start &
