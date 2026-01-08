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