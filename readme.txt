Huong dan su dung

Day la firmware cho khoi Control Power.

Tai lieu serial/PC cu da duoc loai bo.

He thong hien tai:

- Publish trang thai/dashboard len `POWER_CTRL/{UUID}/status`
- Nhan lenh dieu khien va yeu cau doc trang thai tu `POWER_CTRL/{UUID}/command`
- `Opcode = 1`: yeu cau Control Power publish lai snapshot hien tai
- `Opcode = 2`: lenh dieu khien output

Tai lieu dang dung:

- `HUONG_DAN.md`: huong dan van hanh tong quan.
- `MQTT_PROTOCOL.md`: quy uoc topic va payload MQTT cho Control Power.
