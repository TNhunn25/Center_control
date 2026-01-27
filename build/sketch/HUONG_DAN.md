#line 1 "C:\\Users\\Tuyet Nhung-RD\\Desktop\\Project_He_thong_khuech_tan\\master\\master\\HUONG_DAN.md"
# HUONG_DAN_SU_DUNG_HE_THONG_KHUECH_TAN

Tai lieu nay mo ta cach van hanh he thong va giao tiep lenh qua JSON + MD5.
Noi dung duoc tong hop tu code hien tai trong thu muc du an.

## 1) Tong quan
- He thong nhan lenh tu PC qua Serial (UART) theo JSON + MD5.
- Co giao tiep Ethernet (W5500) de ket noi node va dong bo du lieu.
- Ho tro cac lenh dieu khien IO, motor, va lay thong tin tong hop (GET_INFO).

## 2) Chi tiet thiet bi (theo code hien tai)
- Vi dieu khien: ESP32 (co ham ESP.getEfuseMac trong ethernet handler).
- Ethernet: W5500 (SPI).
- IO expander: PCF8575 (doc/ghi input output).
- Cam bien: MICS6814 (NH3/CO/NO2), DHT (temp/humi).
- So luong IO:
  - Output: 4 kenh.
  - Input: 4 kenh.
  - Motor: 5 kenh.
- RS485: co class Rs485Handler (pin mac dinh -1 la chua dung).

## 3) Chan / ket noi quan trong
- Baudrate Serial: 115200.
- Pin cam bien (config.h):
  - PIN_CO = 14
  - PIN_NO2 = 17
  - PIN_NH3 = 18
  - PIN_DHT = 21
  - CONFIG_BUTTON_PIN = 12
- W5500 (comment trong ethernet_handler.cpp):
  - RST = 5, CS = 6, SCK = 7, MISO = 8, MOSI = 9, UDP port = 8888

## 3.1) IP/port mac dinh va cau hinh mang
- IP tinh mac dinh (Ethernet):
  - IP: 192.168.1.100
  - Mask: 255.255.255.0
  - Gateway: 192.168.1.1
  - DNS1: 8.8.8.8
  - DNS2: 1.1.1.1
- UDP listen port: 8888

Cau hinh qua Wi-Fi AP (Config Portal):
- SSID: CENTER-CONFIG
- Password: altacenter
- IP AP: 192.168.4.1
- WebSocket: ws://<ip_ap>:81/


## 4) Giao thuc JSON + MD5
Moi goi lenh co dang:
{
  "id_des": <int>,
  "opcode": <int>,
  "data": {...},
  "time": <unix_time>,
  "auth": "<md5>"
}

Cong thuc auth:
auth = MD5( id_des + opcode + data_json_compact + time + SECRET_KEY )

Phan hoi:
resp_opcode = opcode + 100

## 5) Danh sach opcode
- 1: MIST_COMMAND
- 2: IO_COMMAND
- 3: GET_INFO
- 4: MOTOR_COMMAND
- 5: QUERY_STATUS
- 6: SET_THRESHOLDS

## 6) Trang thai (status)
- 0: khong co loi, goi tin hop le
- 1: loi xac thuc MD5 khong hop le
- 2: loi invalid time (qua han 60s) - can dong bo Unix time
- 3: he thong dang o che do auto/man (tu choi IO_COMMAND khi MAN)
- 255: loi khong xac dinh

Luu y: status = 2 chi co hieu luc neu firmware da implement kiem tra time.

## 7) LED status
He thong chon trang thai LED theo co:
1) has_mode_config == true va has_mode_config_on == false -> STATE_CONFIG_HOLD
2) has_mode_config == false va has_mode_config_on == true -> STATE_CONFIG_ACTIVE
3) has_connect_link == false va has_data_serial == false -> STATE_OFF
4) has_connect_link == false va has_data_serial == true -> STATE_NO_LINK
5) has_connect_link == true va has_data_serial == false -> STATE_NO_DATA_SERIAL
6) Con lai -> STATE_ACTIVE_DATA_ALL

Nhay LED (getBlinkTiming):
- STATE_NORMAL: ON 500ms / OFF 500ms
- STATE_NO_DATA_SERIAL: ON 500ms / OFF 500ms
- STATE_NO_LINK: ON 200ms / OFF 200ms
- STATE_ACTIVE_DATA_ALL: ON 50ms / OFF 950ms
- STATE_CONFIG_HOLD: ON 100ms / OFF 100ms
- STATE_CONFIG_ACTIVE: ON 950ms / OFF 50ms
- STATE_OFF: tat lien tuc

## 8) Huong dan su dung nhanh (PC -> Serial)
1) Ket noi board, mo cong COM (Windows) hoac /dev/ttyUSBx (Linux).
2) Dung script Python:
   - simulator.py: opcode 1/2/3/5/6
   - simulator_motor.py: opcode 1/2/3/4
3) Vi du:
   python simulator.py -p COM21 -o 2 -A --wait 0.8
   python simulator.py -p COM21 -o 3 -i 10
   python simulator_motor.py -p COM21 -o 4 --motor m1,1,0,60

## 9) Goi JSON mau
{"id_des":1,"opcode":2,"data":{"out1":1,"out2":0,"out3":1,"out4":0},"time":1767838338,"auth":"..."}
{"id_des":1,"opcode":4,"data":{"m3":{"run":1,"dir":0,"speed":60}},"time":1760000001,"auth":"..."}

## 10) Kiem tra loi nhanh
- Khong thay phan hoi: kiem tra cong COM, baudrate, va MD5 auth.
- Status = 1: sai SECRET_KEY hoac data_json khong dung thu tu/format.
- Status = 3: he thong dang MAN, IO_COMMAND bi tu choi.

## 11) Thong tin can bo sung (neu co)
- Dien ap nguon, dong tieu thu.
- So do dau noi chi tiet (IO, relay, motor).
- IP tinh/dong, subnet, gateway neu dung Ethernet.
