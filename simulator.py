# import json
# import argparse
# import serial
# import sys
# import time
# import hashlib

# SECRET_KEY = "ALTA_MIST_CONTROLLER"

# def calculate_md5_auth(
#     id_des: int,
#     opcode: int,
#     data: dict,
#     unix_time: int,
#     secret_key: str = SECRET_KEY
# ) -> str:
#     """
#     Tính MD5 auth theo đúng tài liệu: id_des + opcode + json_data_compact + time + key
#     """
#     data_json = json.dumps(data, ensure_ascii=False, separators=(",", ":"))
#     combined = (
#         str(id_des) +
#         str(opcode) +
#         data_json +
#         str(unix_time) +
#         secret_key
#     )
#     return hashlib.md5(combined.encode("utf-8")).hexdigest()

# def send_line(ser: serial.Serial, obj: dict):
#     line = json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n"
#     ser.write(line.encode("utf-8"))
#     ser.flush()

# def read_response(ser: serial.Serial, timeout_s: float):
#     if timeout_s <= 0:
#         return None
#     ser.timeout = timeout_s
#     raw = ser.readline()
#     if not raw:
#         return None
#     return raw.decode("utf-8", errors="ignore").strip()

# def main():
#     ap = argparse.ArgumentParser(description="Gửi lệnh điều khiển theo protocol mới với xác thực MD5")

#     ap.add_argument("-p", "--port", required=True, help="Cổng serial, ví dụ: COM5")
#     ap.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate")
#     ap.add_argument("-d", "--id_des", type=int, default=1, help="ID thiết bị đích (0=all, >=1=cụ thể)")
#     ap.add_argument("-o", "--opcode", type=int, required=True, choices=[1, 2, 3], help="Opcode: 1=MIST_COMMAND, 2=IO_COMMAND, 3=GET_INFO")
#     ap.add_argument("-i", "--interval", type=float, default=1.0, help="Gửi mỗi bao giây")
#     ap.add_argument("--wait", type=float, default=0.5, help="Thời gian chờ phản hồi (giây)")

#     # Đối với opcode 1: MIST_COMMAND
#     ap.add_argument("-n", "--node_id", type=int, help="Node ID (bắt buộc cho opcode 1)")
#     ap.add_argument("--phase1", type=int, default=0, help="Time phase1 (giây, cho opcode 1)")
#     ap.add_argument("--phase2", type=int, default=0, help="Time phase2 (giây, cho opcode 1)")

#     # Đối với opcode 2: IO_COMMAND
#     ap.add_argument("-A", "--on-all", action="store_true", help="Bật tất cả 4 output (cho opcode 2)")
#     ap.add_argument("-X", "--off-all", action="store_true", help="Tắt tất cả 4 output (cho opcode 2)")
#     ap.add_argument("-1", "--out1", action="store_true", help="Bật output 1 (cho opcode 2)")
#     ap.add_argument("-2", "--out2", action="store_true", help="Bật output 2 (cho opcode 2)")
#     ap.add_argument("-3", "--out3", action="store_true", help="Bật output 3 (cho opcode 2)")
#     ap.add_argument("-4", "--out4", action="store_true", help="Bật output 4 (cho opcode 2)")
#     ap.add_argument("-!1", "--no-out1", action="store_true", help="Tắt output 1 (cho opcode 2)")
#     ap.add_argument("-!2", "--no-out2", action="store_true", help="Tắt output 2 (cho opcode 2)")
#     ap.add_argument("-!3", "--no-out3", action="store_true", help="Tắt output 3 (cho opcode 2)")
#     ap.add_argument("-!4", "--no-out4", action="store_true", help="Tắt output 4 (cho opcode 2)")

#     args = ap.parse_args()

#     if args.id_des < 0:
#         print("id_des phải >= 0", file=sys.stderr)
#         sys.exit(2)

#     # Xử lý data dựa trên opcode
#     data = {}
#     if args.opcode == 1:
#         if args.node_id is None:
#             print("Bắt buộc --node_id cho opcode 1", file=sys.stderr)
#             sys.exit(2)
#         data["node_id"] = args.node_id
#         data["time_phase1"] = args.phase1
#         data["time_phase2"] = args.phase2
#     elif args.opcode == 2:
#         if args.on_all and args.off_all:
#             print("Không thể dùng đồng thời --on-all và --off-all", file=sys.stderr)
#             sys.exit(2)

#         outputs = {"out1": 0, "out2": 0, "out3": 0, "out4": 0}
#         if args.on_all:
#             outputs = {"out1": 1, "out2": 1, "out3": 1, "out4": 1}
#         elif args.off_all:
#             outputs = {"out1": 0, "out2": 0, "out3": 0, "out4": 0}

#         if args.out1: outputs["out1"] = 1
#         if args.out2: outputs["out2"] = 1
#         if args.out3: outputs["out3"] = 1
#         if args.out4: outputs["out4"] = 1
#         if args.no_out1: outputs["out1"] = 0
#         if args.no_out2: outputs["out2"] = 0
#         if args.no_out3: outputs["out3"] = 0
#         if args.no_out4: outputs["out4"] = 0

#         data = outputs
#     elif args.opcode == 3:
#         data= {
#             "out1": 0,
#             "out2": 0,
#             "out3": 0,
#             "out4": 0,
#             "in1": 0,
#             "in2": 0,
#             "in3": 0,
#             "in4": 0,
#         }
        

#     print(f"Bắt đầu gửi lệnh opcode {args.opcode} mỗi {args.interval}s đến id_des {args.id_des}")
#     print(f"   Data: {data}")
#     print("Nhấn Ctrl+C để dừng.\n")

#     try:
#         with serial.Serial(args.port, args.baud, timeout=1) as ser:
#             while True:
#                 unix_time = int(time.time())

#                 # Tính auth
#                 auth_hash = calculate_md5_auth(
#                     id_des=args.id_des,
#                     opcode=args.opcode,
#                     data=data,
#                     unix_time=unix_time
#                 )

#                 # Lệnh JSON cuối cùng
#                 cmd = {
#                     "id_des": args.id_des,
#                     "opcode": args.opcode,
#                     "data": data,
#                     "time": unix_time,
#                     "auth": auth_hash
#                 }

#                 send_line(ser, cmd)
#                 print(">", json.dumps(cmd, ensure_ascii=False))

#                 resp = read_response(ser, args.wait)
#                 if resp:
#                     print("<", resp)

#                 time.sleep(args.interval)

#     except KeyboardInterrupt:
#         print("\nĐã dừng bởi người dùng.")
#     except serial.SerialException as e:
#         print(f"Lỗi kết nối serial: {e}", file=sys.stderr)
#         sys.exit(1)

# if __name__ == "__main__":
#     main()





import json
import argparse
import serial
import sys
import time
import hashlib
from typing import List, Optional

SECRET_KEY = "ALTA_MIST_CONTROLLER"


def json_compact(obj: dict, sort_keys: bool = False) -> str:
    """JSON compact đúng style protocol."""
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"), sort_keys=sort_keys)


def calculate_md5_auth(
    id_des: int,
    opcode: int,
    data: dict,
    unix_time: int,
    secret_key: str = SECRET_KEY,
    sort_keys: bool = False,
) -> str:
    """
    MD5 auth: id_des + opcode + json_data_compact + time + key
    """
    data_json = json_compact(data, sort_keys=sort_keys)
    combined = f"{id_des}{opcode}{data_json}{unix_time}{secret_key}"
    return hashlib.md5(combined.encode("utf-8")).hexdigest()


def send_line(ser: serial.Serial, obj: dict, sort_keys: bool = False) -> None:
    line = json_compact(obj, sort_keys=sort_keys) + "\n"
    ser.write(line.encode("utf-8"))
    ser.flush()


def read_all_responses(ser: serial.Serial, timeout_s: float) -> List[str]:
    """
    Đọc hết phản hồi trong khoảng timeout_s (drain).
    Trả về list các dòng đã decode/strip.
    """
    if timeout_s <= 0:
        return []

    end_time = time.time() + timeout_s
    lines: List[str] = []

    # Poll nhanh để gom nhiều line
    old_timeout = ser.timeout
    ser.timeout = 0.05
    try:
        while time.time() < end_time:
            raw = ser.readline()
            if raw:
                lines.append(raw.decode("utf-8", errors="ignore").strip())
            else:
                time.sleep(0.01)
    finally:
        ser.timeout = old_timeout

    return lines


def build_data_from_args(args: argparse.Namespace) -> dict:
    """
    Tạo data dựa trên opcode.
    - opcode 1: MIST_COMMAND
    - opcode 2: IO_COMMAND
    - opcode 3: GET_INFO => data rỗng {}
    opcode 4: MOTOR_COMMAND
    """
    if args.opcode == 1:
        if args.node_id is None:
            raise ValueError("Bắt buộc --node-id cho opcode 1")
        return {
            "node_id": args.node_id,
            "time_phase1": args.phase1,
            "time_phase2": args.phase2,
        }

    if args.opcode == 2:
        if args.on_all and args.off_all:
            raise ValueError("Không thể dùng đồng thời --on-all và --off-all")

        outputs = {"out1": 0, "out2": 0, "out3": 0, "out4": 0}

        if args.on_all:
            outputs = {"out1": 1, "out2": 1, "out3": 1, "out4": 1}
        elif args.off_all:
            outputs = {"out1": 0, "out2": 0, "out3": 0, "out4": 0}

        # ON flags
        if args.out1_on:
            outputs["out1"] = 1
        if args.out2_on:
            outputs["out2"] = 1
        if args.out3_on:
            outputs["out3"] = 1
        if args.out4_on:
            outputs["out4"] = 1

        # OFF flags
        if args.out1_off:
            outputs["out1"] = 0
        if args.out2_off:
            outputs["out2"] = 0
        if args.out3_off:
            outputs["out3"] = 0
        if args.out4_off:
            outputs["out4"] = 0

        return outputs
    
    if args.opcode == 4:
        motor_args = [spec for group in args.motors for spec in group]
        if args.motor_all_run is not None and motor_args:
            raise ValueError("Khong dung chung --motor-all-run va --motor")

        if args.motor_all_run is not None:
            return {f"m{i}": args.motor_all_run for i in range(1, 6)}

        if not motor_args:
            raise ValueError("Bat buoc --motor cho opcode 4")

        motors: dict[str, object] = {}
        for spec in motor_args:
            normalized = spec.replace(":", ",")
            parts = [part.strip() for part in normalized.split(",") if part.strip()]
            if len(parts) not in (2, 4):
                raise ValueError(
                    "Sai format --motor. Vi du: --motor m1,1 hoac --motor m1,1,0,200"
                )

            motor_id = parts[0].lower()
            if motor_id.startswith("m"):
                motor_id = motor_id[1:]
            if not motor_id.isdigit():
                raise ValueError("Motor ID không hợp lệ (m1..m5)")

            motor_index = int(motor_id)
            if motor_index < 1 or motor_index > 5:
                raise ValueError("Motor ID phải từ 1 đến 5")

            try:
                run = int(parts[1])
                if len(parts) == 2:
                    motors[f"m{motor_index}"] = run
                    continue
                direction = int(parts[2])
                speed = int(parts[3])
            except ValueError as exc:
                raise ValueError("run/dir/speed phai la so nguyen") from exc

            motors[f"m{motor_index}"] = {"run": run, "dir": direction, "speed": speed}

        return motors

    # opcode 3: GET_INFO
    return {}


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Gửi lệnh điều khiển theo protocol JSON + MD5 (mặc định gửi 1 gói rồi thoát)"
    )

    ap.add_argument("-p", "--port", required=True, help="Cổng serial, ví dụ: COM5 hoặc /dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate")
    ap.add_argument("-d", "--id-des", type=int, default=1, help="ID thiết bị đích (0=all, >=1=cụ thể)")
    ap.add_argument(
        "-o",
        "--opcode",
        type=int,
        required=True,
        choices=[1, 2, 3, 4],
        help="Opcode: 1=MIST_COMMAND, 2=IO_COMMAND, 3=GET_INFO, 4=MOTOR_COMMAND",
    )

    # Mặc định gửi 1 lần. Nếu muốn lặp, dùng --repeat.
    ap.add_argument("--repeat", type=int, default=1, help="Số lần gửi (mặc định 1). 0 = lặp vô hạn")
    ap.add_argument("-i", "--interval", type=float, default=1.0, help="Khoảng cách giữa các lần gửi khi repeat>1")
    ap.add_argument("--wait", type=float, default=0.5, help="Thời gian chờ/gom phản hồi sau khi gửi (giây)")

    ap.add_argument("--clear-rx", action="store_true", default=True, help="Xoá RX buffer trước khi gửi (mặc định bật)")
    ap.add_argument("--no-clear-rx", dest="clear_rx", action="store_false", help="Không xoá RX buffer trước khi gửi")

    ap.add_argument(
        "--sort-keys",
        action="store_true",
        help="sort_keys khi dump JSON để ổn định MD5 (chỉ bật nếu firmware cũng làm giống)",
    )

    # Opcode 1
    ap.add_argument("-n", "--node-id", type=int, help="Node ID (bắt buộc cho opcode 1)")
    ap.add_argument("--phase1", type=int, default=0, help="Time phase1 (giây, opcode 1)")
    ap.add_argument("--phase2", type=int, default=0, help="Time phase2 (giây, opcode 1)")

    # Opcode 2
    ap.add_argument("-A", "--on-all", action="store_true", help="Bật tất cả 4 output (opcode 2)")
    ap.add_argument("-X", "--off-all", action="store_true", help="Tắt tất cả 4 output (opcode 2)")

    # Rõ ràng ON/OFF (tránh -!1 gây lỗi shell)
    ap.add_argument("--out1-on", action="store_true", help="Bật output 1 (opcode 2)")
    ap.add_argument("--out2-on", action="store_true", help="Bật output 2 (opcode 2)")
    ap.add_argument("--out3-on", action="store_true", help="Bật output 3 (opcode 2)")
    ap.add_argument("--out4-on", action="store_true", help="Bật output 4 (opcode 2)")

    ap.add_argument("--out1-off", action="store_true", help="Tắt output 1 (opcode 2)")
    ap.add_argument("--out2-off", action="store_true", help="Tắt output 2 (opcode 2)")
    ap.add_argument("--out3-off", action="store_true", help="Tắt output 3 (opcode 2)")
    ap.add_argument("--out4-off", action="store_true", help="Tắt output 4 (opcode 2)")

    # Opcode 4
    ap.add_argument(
        "--motor-all-run",
        type=int,
        choices=[0, 1],
        help="Bat/tat dong loat 5 motor (opcode 4). 0=STOP, 1=RUN",
    )

    ap.add_argument(
        "--motor",
        dest="motors",
        action="append",
        nargs="+",
        default=[],
        help=(
            "Motor command (opcode 4). Vi du: --motor m1,1 m2,0 "
            "hoac --motor m1,1,0,200 --motor m2,1,1,128. "
            "Ho tro format m1:1,0,200. Toi da m1..m5. "
            "Dung --motor-all-run 0|1 de bat/tat dong loat 5 motor."
        ),
    )

    args = ap.parse_args()

    if args.id_des < 0:
        print("id_des phải >= 0", file=sys.stderr)
        sys.exit(2)

    try:
        data = build_data_from_args(args)
    except ValueError as e:
        print(str(e), file=sys.stderr)
        sys.exit(2)

    # Thông tin chạy
    mode = "1 lần" if args.repeat == 1 else ("vô hạn" if args.repeat == 0 else f"{args.repeat} lần")
    print(f"Chế độ gửi: {mode}")
    print(f"Port={args.port}, Baud={args.baud}, id_des={args.id_des}, opcode={args.opcode}")
    print(f"Data: {data}")
    print("")

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as ser:
            if args.clear_rx:
                try:
                    ser.reset_input_buffer()
                except Exception:
                    pass

            count = 0
            while True:
                count += 1

                unix_time = int(time.time())
                auth_hash = calculate_md5_auth(
                    id_des=args.id_des,
                    opcode=args.opcode,
                    data=data,
                    unix_time=unix_time,
                    sort_keys=args.sort_keys,
                )

                cmd = {
                    "id_des": args.id_des,
                    "opcode": args.opcode,
                    "data": data,
                    "time": unix_time,
                    "auth": auth_hash,
                }

                send_line(ser, cmd, sort_keys=args.sort_keys)
                print(">", json_compact(cmd, sort_keys=args.sort_keys))

                # Gom hết phản hồi trong wait
                responses = read_all_responses(ser, args.wait)
                for line in responses:
                    print("<", line)

                # Điều kiện dừng
                if args.repeat == 1:
                    break
                if args.repeat > 1 and count >= args.repeat:
                    break

                time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nĐã dừng bởi người dùng.")
    except serial.SerialException as e:
        print(f"Lỗi kết nối serial: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
