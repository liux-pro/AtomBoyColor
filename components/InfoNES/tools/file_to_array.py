with open("contra.nes","rb") as fo:
    read = fo.read()
    with open("rom.c","w") as rom:
        rom.write("#include <stdint.h>\n")
        rom.write("const uint8_t rom[] = {")
        rom.write(",".join([str(hex(x)) for x in read]))
        rom.write("};")