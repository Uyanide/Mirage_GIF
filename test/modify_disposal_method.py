def modify_gif_disposal_method(input_file, output_file, disposal_method):
    try:
        with open(input_file, 'rb') as f_in:
            gif_data = bytearray(f_in.read())

        i = 10
        count = 0
        gct_size = 1 << ((gif_data[i] & 0x07) + 1)
        print(f"全局颜色表大小: {gct_size}")
        i += 3
        if gif_data[10] & 0x80:
            i += gct_size * 3

        def skip_blocks():
            nonlocal i, gif_data
            while gif_data[i] != 0x00:
                i += gif_data[i] + 1
            i += 1

        while i < len(gif_data) - 1:
            print(f"当前块标识符: {gif_data[i]:#04x} {gif_data[i + 1]:#04x}")
            if gif_data[i] == 0x21 and gif_data[i + 1] == 0xF9:
                # GCE
                i += 3  # 跳过 0x21 0xF9 0x04
                gif_data[i] = (gif_data[i] & 0b11100011) | (disposal_method << 2)
                count += 1
                i += 4  # 跳过块数据
                skip_blocks()
            elif gif_data[i] == 0x2C:
                # 图像数据块
                # 跳过图像数据块的本地颜色表（如果存在）
                if gif_data[i + 9] & 0x80:
                    lct_size = 1 << ((gif_data[i + 9] & 0x07) + 1)
                    print(f"局部颜色表大小: {lct_size}")
                else:
                    lct_size = 0
                i += 10  # 头信息固定 10 字节
                i += lct_size * 3  # 跳过局部颜色表
                i += 1  # min code size
                skip_blocks()
            else:
                # 其他块
                i += 2  # 跳过标识
                skip_blocks()

        with open(output_file, 'wb') as f_out:
            f_out.write(gif_data)

        return count
    except Exception as e:
        print(f"错误: {e}")
        return 0


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 4:
        print("用法: python modify_disposal_method.py 输入文件.gif 输出文件.gif 处置方法")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        disposal_method = int(sys.argv[3])
        frames_modified = modify_gif_disposal_method(input_file, output_file, disposal_method)
        print(f"成功修改了 {frames_modified} 帧的处置方法为 {disposal_method}")
