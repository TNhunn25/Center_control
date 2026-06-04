#line 1 "D:\\Power_Central_v4\\MQTT_PROTOCOL.md"
# MQTT Protocol

Tai lieu nay mo ta schema JSON da duoc thong nhat trong firmware `Power_Central`, dong vai tro khoi `Control Power`.

## 1. Schema chung

Moi message JSON gui/nhan qua MQTT uu tien dung khung chung:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Opcode": 1,
  "Data": {},
  "Time": 1767838338,
  "Auth": "md5_optional"
}
```

### Y nghia cac truong

- `Device_ID`: dinh danh thiet bi giao tiep. He thong thong nhat theo format `PDM_<5 so cuoi MAC>`, vi du `PDM_12345`.
- `Opcode`: ma loai ban tin.
- `Data`: du lieu chi tiet theo tung opcode.
- `Time`: Unix timestamp cua goi tin. Truong nay duoc dung de dinh danh ban tin va tranh trung lap. Neu ben gui phai retry cung mot goi do gui that bai, gia tri `Time` nay phai giu nguyen, khong duoc tao lai gia tri moi cho cung noi dung goi tin.
- `Auth`: ma xac thuc trong goi tin, duoc tao tu `MD5(Device_ID + Opcode + DataJson + Time + PRIVATE_KEY)`.

## 2. QoS va Retain

Firmware hien tai da bo ACK muc ung dung tren MQTT. Thay vao do:

- Topic `POWER_CTRL/{UUID}/status` duoc publish voi `retain = true`.
- Topic `POWER_CTRL/{UUID}/command` duoc subscribe voi `QoS 1`.

Luu y ky thuat:

- Thu vien `PubSubClient` dang dung ho tro `retain` khi publish.
- Thu vien nay ho tro subscribe `QoS 1`, nen broker co the giao command o muc `QoS 1`.
- Publish tu firmware van theo co che `QoS 0` cua `PubSubClient`, nen status hien tai la `retain = true`, nhung khong nang len `QoS 1` neu khong doi thu vien MQTT.

## 3. Quy tac tao truong Auth

Firmware MQTT hien tai tinh `Auth` theo mot cong thuc chung cho moi opcode:

```text
Auth = MD5(Device_ID + Opcode + DataJson + Time + PRIVATE_KEY)
```

Luu y quan trong:

- `Auth` KHONG duoc tinh theo kieu `MD5(full_json + PRIVATE_KEY)`.
- Chuoi noi phai dung thu tu: `Device_ID`, `Opcode`, `DataJson`, `Time`, `PRIVATE_KEY`.
- `Opcode` va `Time` phai duoc noi o dang so thap phan, khong co dau nhay kep.
- `PRIVATE_KEY` hien tai duoc quy uoc truoc la `ALTA_2026@` va duoc khai bao trong `config.cpp`.

Quy uoc serialize `DataJson` de tranh sai `Auth`:

- `Data` phai duoc serialize thanh JSON compact, khong them khoang trang va khong xuong dong.
- Thu tu key trong object `Data` phai duoc giu on dinh giua ben gui va ben nhan.
- Gia tri so phai de dang number JSON, khong chuyen thanh string.
- Khong them khoang trang truoc/sau `PRIVATE_KEY`.
- Chuoi dau vao MD5 phai duoc ma hoa bang UTF-8.

Vi du voi command:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Opcode": 2,
  "Data": {
    "out1": 0,
    "out2": 1,
    "out3": 1,
    "out4": 1
  },
  "Time": 1760000000
}
```

Thi chuoi duoc dem di bam phai la:

```text
PDM_12345678901234|2|{"out1":0,"out2":1,"out3":1,"out4":1}|1760000000|PRIVATE_KEY
```

Neu `PRIVATE_KEY = ALTA_2026@` thi chuoi thuc te la:

```text
PDM_12345678901234|2|{"out1":0,"out2":1,"out3":1,"out4":1}|1760000000|ALTA_2026@
```

Khuyen nghi de giam sai khac:

- Ben tao hash va ben verify bat buoc phai dung cung mot cach viet hoa.
- Neu `Data` co truong chuoi nhu `Version`, `State`, `Mode`, thi gia tri chuoi do phai trung khop 100% giua ben gui va ben nhan.

## 4. Telemetry `Opcode = 1` (`DASHBOARD_INFO`)

Topic publish:

```text
POWER_CTRL/{UUID}/status
```

Payload mau:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Opcode": 1,
  "Data": {
    "Version": "1.2.1",
    "State": "On",
    "Mode": "Auto",
    "out1": 0,
    "out2": 1,
    "out3": 0,
    "out4": 1
  },
  "Time": 1767838338,
  "Auth": "md5_optional"
}
```

Quy uoc:

- `Data.Version`: version schema/payload dashboard, hien tai la `1.2.1`.
- `Data.State = On` neu co it nhat mot output dang bat.
- `Data.State = Off` neu tat ca output deu tat.
- `Data.Mode`: `Auto` hoac `Man`.
- `Data.out1..out4`: trang thai 4 output, `1` la ON, `0` la OFF.
- Message nay duoc publish voi `retain = true`.

## 5. Lenh doc trang thai `Opcode = 1` (`DASHBOARD_INFO`)

Topic subscribe:

```text
POWER_CTRL/{UUID}/command
```

QoS subscribe:

```text
QoS 1
```

Payload mau:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Opcode": 1,
  "Data": {},
  "Time": 1767838338,
  "Auth": "md5_optional"
}
```

Luu y:

- Ban tin nay duoc dung de yeu cau `Power_Central` publish lai snapshot dashboard hien tai.
- Firmware se verify `Auth` va `Time` nhu cac opcode khac.
- Sau khi nhan goi hop le, firmware publish trang thai len topic `POWER_CTRL/{UUID}/status`.

## 6. Command `Opcode = 2` (`IO_COMMAND`)

Topic subscribe:

```text
POWER_CTRL/{UUID}/command
```

QoS subscribe:

```text
QoS 1
```

Payload central firmware dang parse:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Opcode": 2,
  "Data": {
    "out1": 0,
    "out2": 1,
    "out3": 0,
    "out4": 1
  },
  "Time": 1767838338,
  "Auth": "md5_optional"
}
```

Luu y:

- Firmware xu ly bat buoc `Data` phai co du `out1..out4`.
- Khung command khong can `Mode`, `Power_State`, `Seq` o top-level.
- Sau khi nhan command, firmware khong publish MQTT ACK rieng.

## 7. Topic su dung

- Publish trang thai: `POWER_CTRL/{UUID}/status`
- Subscribe lenh dieu khien va doc trang thai: `POWER_CTRL/{UUID}/command`
- Publish trang thai online/last will: `POWER_CTRL/{UUID}/connection`

## 8. Trang thai online

Firmware publish trang thai connection/last will len topic:

```text
POWER_CTRL/{UUID}/connection
```

Payload mau:

```json
{
  "Device_ID": "PDM_12345678901234",
  "Status": "Online",
  "Time": 1767838338
}
```

Quy uoc:

- Day la goi last will/status connection, payload khong con truong `UUID`.
- Khi ket noi MQTT thanh cong, firmware publish `Status = Online` voi `retain = true`.
- Khi mat ket noi bat thuong, MQTT Last Will publish `Status = Offline` voi `retain = true`.
