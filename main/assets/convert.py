import cv2
import numpy as np

# 导入 stdint.h 头文件
HEADER = '''#include <stdint.h>

'''

# 读取图片并调整大小
img = cv2.imread('logo.jpg')
img = cv2.resize(img, (240, 240))
img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

# 将图片转换为 RGB565 格式
rgb565_data = np.zeros((240, 240), dtype=np.uint16)
for y in range(240):
    for x in range(240):
        r, g, b = img[y, x]
        r >>= 3
        g >>= 2
        b >>= 3
        rgb565 = (r << 11) | (g << 5) | b
        rgb565_data[y, x] = rgb565

# 将 RGB565 数据输出为 C 语言数组
with open('logo.c', 'w') as f:
    f.write(HEADER)
    f.write('const uint8_t logo[] = {\n')
    for y in range(240):
        for x in range(0, 240):
            byte1 = (rgb565_data[y, x] >> 8) & 0xFF
            byte2 = rgb565_data[y, x] & 0xFF
            # swap
            f.write(f'0x{byte1:02x},  0x{byte2:02x},')
        f.write('\n')
    f.write('};\n')
