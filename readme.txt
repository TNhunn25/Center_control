# Hướng dẫn sử dụng

Tài liệu này mô tả cách chạy các script mô phỏng (simulator) để gửi lệnh điều khiển theo giao thức JSON + MD5 qua cổng serial.

## Yêu cầu

- Python 3.8+.
- Thư viện `pyserial` để giao tiếp serial.

Cài đặt nhanh:

```bash
python -m pip install pyserial
```

## Tổng quan script

- `simulator.py`: gửi lệnh opcode 1/2/3/5/6 theo chu kỳ (lặp vô hạn cho đến khi dừng).
- `simulator_motor.py`: gửi lệnh opcode 1/2/3/4; mặc định gửi 1 lần, có thể lặp bằng `--repeat`.

## Cú pháp chung

> **Lưu ý**: Cổng serial có thể là `COMx` (Windows) hoặc `/dev/ttyUSBx` (Linux).

### 1) `simulator.py`

```bash
python simulator.py -p <PORT> -o <OPCODE> [tùy chọn khác]
```

Tùy chọn chính:

- `-p/--port`: cổng serial (bắt buộc).
- `-b/--baud`: baudrate (mặc định `115200`).
- `-d/--id_des`: ID thiết bị đích (mặc định `1`, `0` = broadcast).
- `-o/--opcode`: opcode điều khiển (`1,2,3,5,6`).
- `-i/--interval`: chu kỳ gửi lệnh (giây).
- `--wait`: thời gian chờ phản hồi (giây).

#### Opcode 1 (MIST_COMMAND)

Bắt buộc `--node_id` và có thể thêm `--phase1`, `--phase2`.

```bash
python simulator.py -p COM21 -o 1 -n 0 --phase1 2 --phase2 12 -i 5
```

#### Opcode 2 (IO_COMMAND)

Các cờ bật/tắt output:

- `-A/--on-all`: bật tất cả output.
- `-X/--off-all`: tắt tất cả output.
- `-1/--out1`, `-2/--out2`, `-3/--out3`, `-4/--out4`: bật output riêng lẻ.
- `-!1/--no-out1`, `-!2/--no-out2`, `-!3/--no-out3`, `-!4/--no-out4`: tắt output riêng lẻ.

Ví dụ bật/tắt:

```bash
python simulator.py -p COM21 -o 2 -1 -2 -3 -4 -i 5
python simulator.py -p COM10 -o 2 -A --wait 0.8
```

#### Opcode 3 (GET_INFO)

```bash
python simulator.py -p COM21 -o 3 -i 10
```

Nếu cần lặp hữu hạn, hãy dùng `simulator_motor.py` với `--repeat` (ví dụ ở phần bên dưới).

#### Opcode 5 (QUERY_STATUS)

```bash
python simulator.py -p COM21 -o 5 -n 0 -i 2
```

#### Opcode 6 (SET_THRESHOLDS)

Yêu cầu đủ 10 ngưỡng:

```bash
python simulator.py -p COM21 -o 6 \
  --temp_on 55 --temp_off 45 \
  --humi_on 54 --humi_off 50 \
  --nh3_on 50 --nh3_off 45 \
  --co_on 6 --co_off 5 \
  --no2_on 50 --no2_off 40 \
  -i 5
```

### 2) `simulator_motor.py`

```bash
python simulator_motor.py -p <PORT> -o <OPCODE> [tùy chọn khác]
```

Tùy chọn chính:

- `-p/--port`, `-b/--baud`, `-d/--id-des`, `-o/--opcode` tương tự `simulator.py`.
- `--repeat`: số lần gửi (mặc định `1`, `0` = lặp vô hạn).
- `-i/--interval`: khoảng cách giữa các lần gửi khi `repeat > 1`.
- `--wait`: thời gian gom phản hồi sau khi gửi.
- `--clear-rx` / `--no-clear-rx`: xoá/giữ RX buffer trước khi gửi.

#### Opcode 4 (MOTOR_COMMAND)

Bật/tắt tất cả motor:

```bash
python simulator_motor.py -p COM21 -o 4 --motor-all-run 1
```

Điều khiển từng motor:

```bash
python simulator_motor.py -p COM21 -o 4 --motor m1,1,0,0 --repeat 10 -i 5
```

Lặp GET_INFO 10 lần:

```bash
python simulator_motor.py -p COM10 -o 3 --repeat 10 -i 1 --wait 0.5
```

- `--motor m1,1`: chạy motor 1 (run=1).
- `--motor m1,1,0,200`: chạy motor 1, dir=0, speed=200.

## Ví dụ gói JSON mẫu

```json
{"id_des":1,"opcode":2,"data":{"out1":1,"out2":0,"out3":1,"out4":0},"time":1767838338,"auth":"c5ee94ad4c238b2f19d860844d8ac372"}
```

```json
{"id_des":1,"opcode":4,"data":{"m1":1,"m2":1,"m3":0,"m4":1,"m5":1},"time":1760000000,"auth":"2df11e5016f6c9867775e7250e7851a4"}
```

```json
{"id_des":1,"opcode":4,"data":{"m3":{"run":1,"dir":0,"speed":60}},"time":1760000001,"auth":"d94468587cca5a4aae96759505a4ca76"}
```

## Lưu ý vận hành

- Dừng script bằng `Ctrl+C`.
- Nếu bị lỗi kết nối serial, kiểm tra lại cổng (`-p`) và quyền truy cập thiết bị.
- Mặc định các script sẽ in ra lệnh đã gửi và phản hồi (nếu có) để dễ theo dõi.

## Hoạt động của LED status

`LedStatus::update()` chọn trạng thái LED dựa trên các cờ hệ thống (từ `config.h`), theo thứ tự ưu tiên:

1. `has_mode_config == true` và `has_mode_config_on == false` → `STATE_CONFIG_HOLD`.
2. `has_mode_config == false` và `has_mode_config_on == true` → `STATE_CONFIG_ACTIVE`.
3. `has_connect_link == false` và `has_data_serial == false` → `STATE_OFF`.
4. `has_connect_link == false` và `has_data_serial == true` → `STATE_NO_LINK`.
5. `has_connect_link == true` và `has_data_serial == false` → `STATE_NO_DATA_SERIAL`.
6. Còn lại → `STATE_ACTIVE_DATA_ALL`.  

Khi state thay đổi, LED sẽ reset chu kỳ nháy và bật ngay 1 nhịp. Riêng `STATE_OFF` thì LED tắt hẳn.  

Bảng nháy theo `getBlinkTiming()`:

- `STATE_NORMAL`: ON 500ms / OFF 500ms.
- `STATE_NO_DATA_SERIAL`: ON 500ms / OFF 500ms.
- `STATE_NO_LINK`: ON 200ms / OFF 200ms.
- `STATE_ACTIVE_DATA_ALL`: ON 50ms / OFF 950ms.
- `STATE_CONFIG_HOLD`: ON 100ms / OFF 100ms.
- `STATE_CONFIG_ACTIVE`: ON 950ms / OFF 50ms.
- `STATE_OFF`: tắt liên tục.