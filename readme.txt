Code chạy giả lập PC 

python simulator.py -p COM10 -o 1 -n 3 --phase1 2 --phase2 12 -i 1

python simulator.py -p COM10 -o 2 -n 0 -1 -2 -3 -4 -i 2


////////////////////////////////////////////////////////////////////
code mới 

IO_COMMAND bật out1 + out3 (gửi 1 lần)-----------------------------

python simulator.py -p COM10 -o 2 --out1-on --out3-on --wait 0.8


IO_COMMAND bật tất cả (gửi 1 lần)----------------------------------

python simulator.py -p COM10 -o 2 -A --wait 0.8

GET_INFO (gửi 1 lần)----------------------------------------------

python simulator.py -p COM10 -o 3 --wait 1.0

Nếu cần lặp 10 lần (không còn while true “cứng”)------------------

python simulator.py -p COM10 -o 3 --repeat 10 -i 1 --wait 0.5

---------------------------
python simulator.py -p COM10 -o 1 -n 3 --phase1 2 --phase2 12 -i 1


{"id_des":1,"opcode":2,"data":{"out1":1,"out2":0,"out3":1,"out4":0},"time":1767838338,"auth":"c5ee94ad4c238b2f19d860844d8ac372"}


{"id_des": 1,"opcode": 4,"data": {"m3": {"run": 1, "dir": 0, "speed": 0}},"time": 1760000001,"auth": "97725122c43ebb1e4fb34c613bb97cc1"}

{"id_des": 1,"opcode": 4,"data": {"m1": {"run": 1, "dir": 0, "speed": 200},"m2": {"run": 1, "dir": 1, "speed": 128},"m3": {"run": 0, "dir": 0, "speed": 0},"m4": {"run": 1, "dir": 0, "speed": 255},"m5": {"run": 1, "dir": 1, "speed": 40}},"time": 1760000000,"auth": " 94aadd4ad767a91b87aacc5351856e54"}	

