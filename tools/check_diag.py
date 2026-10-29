import serial
import time

ser = serial.Serial('COM12', 115200, timeout=1.5)
time.sleep(1.0)
ser.reset_input_buffer()

for cmd in ['ID', 'RPROG:8006', 'RPROG:8007', 'RPROG:8008', 'RPROG:0000', 'RPROG:0001', 'RPROG:0002', 'RPROG:0003', 'RPROG:0004', 'DONE']:
    ser.write((cmd + '\n').encode())
    resp = ser.readline().decode('utf-8', errors='replace').strip()
    print(f"{cmd:12} -> {resp}")

ser.close()
