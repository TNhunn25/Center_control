import json
import argparse
import serial
import sys
import time
import hashlib

SECRET_KEY = "ALTA_MIST_CONTROLLER"

def calculate_md5_auth(
    id_des: int,
    opcode: int,
    data: dict,
    unix_time: int,
    secret_key: str = SECRET_KEY
) -> str:
    """
    Tính MD5 auth theo đúng tài liệu: id_des + opcode + json_data_compact + time + key
    """
    data_json = json.dumps(data, ensure_ascii=False, separators=(",", ":"))
    combined = (
        str(id_des) +
        str(opcode) +
        data_json +
        str(unix_time) +
        secret_key
    )
    return hashlib.md5(combined.encode("utf-8")).hexdigest()

def send_line(ser: serial.Serial, obj: dict):
    line = json.dumps(obj, ensure_ascii=False, separators=(",", ":")) + "\n"
    ser.write(line.encode("utf-8"))
    ser.flush()

def read_response(ser: serial.Serial, timeout_s: float):
    if timeout_s <= 0:
        return None
    ser.timeout = timeout_s
    raw = ser.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="ignore").strip()

def main():
    ap = argparse.ArgumentParser(description="Gửi lệnh điều khiển theo protocol mới với xác thực MD5")

    ap.add_argument("-p", "--port", required=True, help="Cổng serial, ví dụ: COM5")
    ap.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate")
    ap.add_argument("-d", "--id_des", type=int, default=1, help="ID thiết bị đích (0=all, >=1=cụ thể)")
    ap.add_argument("-o", "--opcode", type=int, required=True, choices=[1, 2], help="Opcode: 1=MIST_COMMAND, 2=IO_COMMAND")
    ap.add_argument("-i", "--interval", type=float, default=1.0, help="Gửi mỗi bao giây")
    ap.add_argument("--wait", type=float, default=0.5, help="Thời gian chờ phản hồi (giây)")

    # Đối với opcode 1: MIST_COMMAND
    ap.add_argument("-n", "--node_id", type=int, help="Node ID (bắt buộc cho opcode 1)")
    ap.add_argument("--phase1", type=int, default=0, help="Time phase1 (giây, cho opcode 1)")
    ap.add_argument("--phase2", type=int, default=0, help="Time phase2 (giây, cho opcode 1)")

    # Đối với opcode 2: IO_COMMAND
    ap.add_argument("-A", "--on-all", action="store_true", help="Bật tất cả 4 output (cho opcode 2)")
    ap.add_argument("-X", "--off-all", action="store_true", help="Tắt tất cả 4 output (cho opcode 2)")
    ap.add_argument("-1", "--out1", action="store_true", help="Bật output 1 (cho opcode 2)")
    ap.add_argument("-2", "--out2", action="store_true", help="Bật output 2 (cho opcode 2)")
    ap.add_argument("-3", "--out3", action="store_true", help="Bật output 3 (cho opcode 2)")
    ap.add_argument("-4", "--out4", action="store_true", help="Bật output 4 (cho opcode 2)")
    ap.add_argument("-!1", "--no-out1", action="store_true", help="Tắt output 1 (cho opcode 2)")
    ap.add_argument("-!2", "--no-out2", action="store_true", help="Tắt output 2 (cho opcode 2)")
    ap.add_argument("-!3", "--no-out3", action="store_true", help="Tắt output 3 (cho opcode 2)")
    ap.add_argument("-!4", "--no-out4", action="store_true", help="Tắt output 4 (cho opcode 2)")

    args = ap.parse_args()

    if args.id_des < 0:
        print("id_des phải >= 0", file=sys.stderr)
        sys.exit(2)

    # Xử lý data dựa trên opcode
    data = {}
    if args.opcode == 1:
        if args.node_id is None:
            print("Bắt buộc --node_id cho opcode 1", file=sys.stderr)
            sys.exit(2)
        data["node_id"] = args.node_id
        data["time_phase1"] = args.phase1
        data["time_phase2"] = args.phase2
    elif args.opcode == 2:
        if args.on_all and args.off_all:
            print("Không thể dùng đồng thời --on-all và --off-all", file=sys.stderr)
            sys.exit(2)

        outputs = {"out1": False, "out2": False, "out3": False, "out4": False}
        if args.on_all:
            outputs = {"out1": True, "out2": True, "out3": True, "out4": True}
        elif args.off_all:
            outputs = {"out1": False, "out2": False, "out3": False, "out4": False}

        if args.out1: outputs["out1"] = True
        if args.out2: outputs["out2"] = True
        if args.out3: outputs["out3"] = True
        if args.out4: outputs["out4"] = True
        if args.no_out1: outputs["out1"] = False
        if args.no_out2: outputs["out2"] = False
        if args.no_out3: outputs["out3"] = False
        if args.no_out4: outputs["out4"] = False

        data = outputs

    print(f"Bắt đầu gửi lệnh opcode {args.opcode} mỗi {args.interval}s đến id_des {args.id_des}")
    print(f"   Data: {data}")
    print("Nhấn Ctrl+C để dừng.\n")

    try:
        with serial.Serial(args.port, args.baud, timeout=1) as ser:
            while True:
                unix_time = int(time.time())

                # Tính auth
                auth_hash = calculate_md5_auth(
                    id_des=args.id_des,
                    opcode=args.opcode,
                    data=data,
                    unix_time=unix_time
                )

                # Lệnh JSON cuối cùng
                cmd = {
                    "id_des": args.id_des,
                    "opcode": args.opcode,
                    "data": data,
                    "time": unix_time,
                    "auth": auth_hash
                }

                send_line(ser, cmd)
                print(">", json.dumps(cmd, ensure_ascii=False))

                resp = read_response(ser, args.wait)
                if resp:
                    print("<", resp)

                time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nĐã dừng bởi người dùng.")
    except serial.SerialException as e:
        print(f"Lỗi kết nối serial: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()