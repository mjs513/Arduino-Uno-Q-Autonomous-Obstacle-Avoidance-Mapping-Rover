# Arduino-Uno-Q-Autonomous-Obstacle-Avoidance-Mapping-Rover

This project is meant to serve two purposes:

1.  Continuation of my robotics journey that minimizes sensors needed for obstacle avoidance, skimpily obstacle avoidance algorithm, and add support for SLAM.
2. Maximize the use of the Arduino Uno Q to use both the MCU and MPU based on several things with documented on the Arduino Uno Q forum:
- [Anyone had luck reading from a gamepad? UNO Family / UNO Q Arduino Forum](https://forum.arduino.cc/t/anyone-had-luck-reading-from-a-gamepad/1436918) – custom brick for PS4/Xbox joysticks
- [Getting a Neato XV-11 Lidar Working on the Q UNO Family / UNO Q Arduino Forum](https://forum.arduino.cc/t/getting-a-neato-xv-11-lidar-working-on-the-q/1445568)
- [Substitute for k_thread_define when creating threads UNO Family / UNO Q Arduino Forum](https://forum.arduino.cc/t/substitute-for-k-thread-define-when-creating-threads/1452414)
- [Bridge call or notify and Threading UNO Family / UNO Q Arduino Forum](https://forum.arduino.cc/t/bridge-call-or-notify-and-threading/1451225/6)

**HARDWARE** **– Parts List**

| Item | Image | Link                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| :--- | :---- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1    |   ![Rover](images/rover.jpg)    | [Amazon.com: Robot Tracked Tank Car Chassis with Encoder DC Motor, Suspension Shock Absorption Chassis Car Full-Metal Robotic Moving Platform Track for Arduion RPi Microb Jetson Python DIY Smart Robot Maker : Industrial & Scientific](https://www.amazon.com/dp/B0CPJFY3BS?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1&th=1)                                                                                                                                                                                                                                                                                                                                                                                                                |
| 2    |       | [Screw Terminal Block Breakout Shield Module for Arduino UNO R3: Amazon.com: Industrial & Scientific ](https://www.amazon.com/dp/B08LH5TCM5?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_77)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| 3    |   ![Terminal](images/terminalShield.jpg)     | [Amazon.com: Arduino Motor Shield REV3 [A000079] - Motor Control, 4 DC Motors, 2 Stepper Motors, 1.2A per Motor, Integrated Power Supply, Compatible with Arduino IDE for Robotics and Automation Projects : Electronics](https://www.amazon.com/Arduino-A000079-Motor-Shield-12V/dp/B0084ZQF3O/ref=sr_1_1?crid=XJQRNPCAVYQ9&dib=eyJ2IjoiMSJ9.8-4UuboGx363PCiFqpqQLD1Hd9Q__KQPOr39CWg2lZAhC-0bLX-ZdPHapq9uhi0K9HzHrAwLf-HZDEwcFClbhS0K3ugk6YQDukwLTSmr1oA9SkmDy4DsgNd7vbWYXkK0P6DBHM4Rd2GQHTTTVu9McHiC5ogFqYQKVpgPtJ7UW6X7udCL9FdyExWgGTsf91uvt8GWMZB4ohr-weCbqTex7bM17c_V_FJt2eggVfwG3Ls.kMdslqvOmtzBqSVxOM-B5FydZhbicXg3S8tHbnIBodc&dib_tag=se&keywords=arduino+motor+shield+rev3&qid=1792509400&sprefix=arduino+motor+shi%2Caps%2C148&sr=8-1) |
| 4    |    | [Adafruit 9-DOF Absolute Orientation IMU Fusion Breakout - BNO055 [STEMMA QT / Qwiic] : Adafruit Industries, Unique & fun DIY electronics and kits](https://www.adafruit.com/product/4646)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| 5    |       | [SparkFun Qwiic ToF Imager - VL53L5CX - SparkFun Electronics](https://www.sparkfun.com/sparkfun-qwiic-tof-imager-vl53l5cx.html)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| 6    |       | [Amazon.com: 2Pairs Deans-T Plug to 2-PIN JST-PH2.0 Male Female Adapter Conversion Cable/Lead/Wire 22AWG 100mm for ESC 1/24 1/18 RC Crawler SCX24 AX24 TRX4M FCX24 : Toys & Games  ](https://www.amazon.com/dp/B0F322H2ZY?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2)                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| 7    |       | [ Amazon.com: 10Pair JST-PH 2.0mm 6Pins Connector, Male and Female Plug Jack Cable Connector 2.0mm Pitch 26AWG Cables 20CM : Electronics   ](https://www.amazon.com/dp/B0F322H2ZY?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_2)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| 8    |       | [Amazon.com: Raogoodcx 360pcs M2 M3 M4 Male Female Nylon Hex Spacer Standoff Screw Nut Assortment Kit : Industrial & Scientific](https://www.amazon.com/dp/B01H03WWD2?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| 9    |       | [Amazon.com: I2C Qwiic Cable Kit Stemma QT Wire for SparkFun Development Boards Sensor Board Breakout Breadboard 4 Pin Sh1.0 Connector : Industrial & Scientific ](https://www.amazon.com/dp/B08HQ1VSVL?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_14)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| 10   |       | [SparkFun Qwiic Adapter - SparkFun Electronics](https://www.sparkfun.com/sparkfun-qwiic-adapter.html)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 11   |       | [Amazon.com: 6 inch Short Fast Charging Cord, 5 Pack Durable USB A to USB Type C 3A Cable for Charging Station Compatible with Samsung Galaxy Note 9 10 S10 S20 S30 OnePlus 7T 8T LG V30 V40 : Electronics  ](https://www.amazon.com/dp/B08LL1SVZD?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1&th=1)                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 12   |       | [Amazon.com: OVONIC 3S 11.1V Lipo Battery 2200mAh 35C Max to 70C Deans Connector - 2 Pack 3S Short Packs Batteries with Hyper Power for RC Car Vehicles RC Boat Drone Airplane Quadcopter Helicopter FPV (2 Packs) : Toys & Games](https://www.amazon.com/dp/B08R6V9FGW?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_4)                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 13   |       | [Amazon.com: MRBIU 5v 2a Heated Vest Battery Pack, 10000mah LCD Display Power Bank for Heated Jacket,Heated Coat,Heated Clothing,Heated Pant,Heated Hoodie,USB Heated Blanket,Heated Seat/Chair. : Cell Phones & Accessories](https://www.amazon.com/dp/B08YNQPKFJ?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1)                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 14   |       | [Amazon.com: Lswteiz 4Pack White ABS Plastic Sheet 12" x 16" x 1/8" Thick White Rigid Thermoplastic Sheet (Upgrade Surface) 3MM Flexible High Tensile ABS Moldable Plastic Panel for Crafts, DIY, Signs : Industrial & Scientific](https://www.amazon.com/dp/B0GK1Q6QK5?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_3)                                                                                                                                                                                                                                                                                                                                                                                                                            |
| 15   |       | [Amazon.com: 1110PCS Nuts and Bolts Assortment Kit, Stainless Steel Hardware Assortment Kit with Assorted Screws, Nuts, Bolts and Washers（#4-40#6-32#8-32#10-24） : Industrial & Scientific ](https://www.amazon.com/dp/B0C5N5VSJ2?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_12)                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| 16   |       | [Amazon.com: Honbay CNC Aluminum GPS Folding Antenna Base Mount Holder Bracket for DJI Quadcopter Multicopter (Black) : Toys & Game](https://www.amazon.com/dp/B07WPCY7K5?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_1)s                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| 17   |       | [Amazon.com: 3DR 500MW Radio Telemetry Kit 915Mhz 915 Kit Air and Groud Data Transmit Module for APM2.6 APM2.8 PX4 Pixhawk : Toys & Games ](https://www.amazon.com/Soulload-Telemetry-915Mhz-Transmit-Pixhawk/dp/B0768WQ989/ref=sr_1_1?crid=EWVINNVSWAE0&dib=eyJ2IjoiMSJ9.36pUdBxc2OUuRT8z1d-z98BdINikfFt_Rl9lwPVTthjgC_L__eSpDWTD1ZFuiHFtQQuaxMUF8Vw_Z20SU1bPkQAA-j-HPxIlzJtSkIb7UBlZdb-LADqp8vMCSRT8E_CDnui9dvIJXCWgSWYrkl0fxzOg4IkdMaFpTnJ1A8Ok111ZyUUaASgav00U9vSHdsimlf5UXbcD5V5RZg_A1AtqALeASSv1Z7Lniy3bYkf7HAtUap5JQ-Ml8hRLr6ZWi4x37YnpPE0n1Xk4Z7ebZChRy8aMePTvVA3bQu5wYEkgSmw.6Ij0jjNLmJHGn3ny4IAxJJLoG-fZX018qtv34pVqJtk&dib_tag=se&keywords=3dr+915mhz+telemetry+radio&qid=1792513603&sprefix=3dr+radio%2Caps%2C134&sr=8-1)            |
| 18   |       | XV-11 Neato Lidar plus GetSurreal controller and mount.  Getsurreal is now defunct so the closet thing you are going to get is RPILIdar but you will probably have to modify the code base to get it working correctly.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |


## **ASSEMBLY**

![RoverSide](images/RoverSide.jpg) 
![RoverFront](images/RoverFront.jpg) 

**BUILD COMMENTS:**

1.  The battery (12) has a Deans-T connector so I used the Deans-T Plug to 2-PIN JST-PH2.0 (6) to the motor shield where I used a female jst to connect to the jst cable I used with the battery.  I cut the other end off and attached it to the Motore Shield

![PowerConnector](images/PowerConnector.jpg) 

3. On the motor shield I cut the Vin trace so the battery (12) doesn’t supply power to the Arduino Uno Q
   ![RoverSide](images/MotorShieldVin.png) 

4. The chassis comes with wiring for the Motors that have a 6 pin jstconnector on them. I used the 6 pin cables (7) to connector from the motor wiring to extend them and attach them to the terminal shield.
![MotorConnector](images/MotorConnector.jpg) 

**Wire Color Coding and Pin Connection**
  
![Schematic](images/TerminalShieldSch.jpg) 
Figure  Terminal Shield

 
![MotorShield Power Pins](images/MotorShield.jpg) 
Figure  Motor Shield


| **Motor** | **Color** | **Function** | **Pin**                       |
| :-------- | :-------- | :----------- | :---------------------------- |
| Left      | Black     | M+           | Motor Shield Pin 1            |
|           | Greeb     | M-           | Motor Shield Pin 2            |
|           | Red       | Gnd          | Encoder Gnd                   |
|           | Orange    | 3.3v         | Encoder Pwr                   |
|           | White     | 2            | Encoder                       |
|           | Yellow    | 4            | Encorder                      |
| Right     | Black     | M+           | Motor Shield Pin 3            |
|           | Green     | M-           | Motor Shield Pin 4            |
|           | Red       | Gnd          | Encoder Gnd – Terminal Shield |
|           | Orange    | 3.3v         | Encoder Pwr – Terminal Shield |
|           | White     | 6            | Encoder – Terminal Shield     |
|           | Yellow    | 7            | Encorder – Terminal Shield    |


**4 LIDAR Connections**

| **Color** | **Function** | **Pin**                               |
| :-------- | :----------- | :------------------------------------ |
| Red       | PWR          | 5v pin on the terminal shiled header  |
| Brown     | GND          | GND pin on the terminal shield header |
| Orange    | 5            | PWM pin on the terminal shield header |
| Yellow    | Serial3      | SDA Pin on the terminal shield header |

**5. 3dr Radio**

| **Color** | **Function** | **Pin**                               |
| :-------- | :----------- | :------------------------------------ |
| White     | PWR          | 5v pin on the terminal shiled header  |
| Black     | GND          | GND pin on the terminal shield header |
| Orange    | 1            | RX on terminal shield                 |
| Yellow    | 0            | TX on terminal shield                 |

**6. BNO055 IMU and VL53 on Wire1**
![QWIIC Adapter](images/QwiicAdapt.png) 

| **Device** | **Color**         | **Function** | **Pin**                                      |
| :--------- | :---------------- | :----------- | :------------------------------------------- |
| BNO055     | Orange            | 3.3V         | 3.3v pin                                     |
|            | Blue              | GND          | GND pin                                      |
|            | Green             | SDA          | SDA PIN on Adapter                           |
|            | Yellow            | SCL          | SCL Pin on Adapter                           |
| VL53L05CX  | Long Qwiic Cable  | Wire1        | Cable from Device to QWIIC Adapter           |
| Adapter    | Short QWIIC Cable | Wire1        | Short QWIIC cable from Adapter to Wire1 on Q |

7.	The abs is used to create mounting plates for the Q/3dr Radio/ VL53/Lidar.  Cut to size you want to use to mount your hardware.





