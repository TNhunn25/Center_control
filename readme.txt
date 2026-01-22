Code chạy giả lập PC 

python simulator.py -p COM21 -o 1 -n 0 --phase1 2 --phase2 12 -i 5

python simulator.py -p COM21 -o 2 -1 -2 -3 -4 -i 5

python simulator.py -p COM21 -o 3 -i 10

python simulator.py -p COM21 -o 6 --temp_on 55 --temp_off 45 --humi_on 54 --humi_off 50 --nh3_on 50 --nh3_off 45 --co_on 50 --co_off 45 --no2_on 50 --no2_off 40 -i 5

python simulator.py -p COM21 -o 5 -n 0  -i 2

python simulator_motor.py -p COM21 -o 4 --motor m1,1,0,0  --repeat 10 -i 5

IO_COMMAND bật tất cả (gửi 1 lần)----------------------------------

python simulator.py -p COM21 -o 1 -n 0 --phase1 0 --phase2 2  -i 25 //test opcode 1 

python simulator.py -p COM10 -o 2 -A --wait 0.8

GET_INFO (gửi 1 lần)----------------------------------------------

python simulator.py -p COM21 -o 3 -i 10

Nếu cần lặp 10 lần (không còn while true “cứng”)------------------

python simulator.py -p COM10 -o 3 --repeat 10 -i 1 --wait 0.5

---------------------------
Lệnh opcode 2
{"id_des":1,"opcode":2,"data":{"out1":1,"out2":0,"out3":1,"out4":0},"time":1767838338,"auth":"c5ee94ad4c238b2f19d860844d8ac372"}

Lệnh opcode 4:
{"id_des": 1,"opcode": 4,"data": {"m1": 1,"m2": 1,"m3": 0,"m4": 1,"m5": 1},"time": 1760000000,"auth":"2df11e5016f6c9867775e7250e7851a4"}

{"id_des": 1,"opcode": 4,"data": {"m3": {"run": 1, "dir": 0, "speed": 60}},"time": 1760000001,"auth": "d94468587cca5a4aae96759505a4ca76"}
-----------------------------------------------------------------------




