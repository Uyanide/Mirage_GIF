from PIL import Image, ImageSequence
from tqdm import tqdm
import numpy as np


_BAYER_MATRIX = np.array([
    [0,  8,  2, 10],
    [12,  4, 14,  6],
    [3, 11,  1,  9],
    [15,  7, 13,  5]
]) / 16.0 * 255.0
_TRANS_COLOR = 100
_ENABLE_LOG = True


def _log(*args, **kwargs):
    if _ENABLE_LOG:
        print(*args, **kwargs)


def enable_log(enable: bool = True):
    '''
    启用或禁用日志输出。

    :param enable: 是否启用日志输出，默认为True
    '''
    global _ENABLE_LOG
    _ENABLE_LOG = enable


class _Progress_Bar:
    def __init__(self, total, desc, unit, leave, update_step):
        self.update_step = update_step
        if _ENABLE_LOG:
            self._bar = tqdm(total=total, desc=desc, unit=unit, leave=leave)

    def update(self, update_step=None):
        if not _ENABLE_LOG:
            return
        if update_step is None:
            update_step = self.update_step
        self._bar.update(update_step)

    def close(self):
        if _ENABLE_LOG:
            self._bar.close()


def _read_gif(path_i: str, path_c: str, max_size: int, max_frames: int) -> tuple:
    '''
    读取两个GIF文件，返回两个GIF的帧列表和帧间隔。

    首先里GIF会被缩放到长宽均不超过max_size，然后表GIF会被裁剪和缩放到与里GIF相同的尺寸以完整覆盖。

    帧数会被统一为两个GIF中较高的帧数，且不超过max_frames。不足目标帧数的GIF会被重复播放直至达到目标帧数。

    帧间隔会取两个GIF中的最大值。

    :param path_i: 里GIF路径
    :param path_c: 表GIF路径
    :param max_size: 最大尺寸
    :param max_frames: 最大帧数

    :return: 内部帧列表，外部帧列表，帧间隔
    '''
    imgs = [Image.open(path_i), Image.open(path_c)]
    inner_frames = [frame.copy() for frame in ImageSequence.Iterator(imgs[0])]
    cover_frames = [frame.copy() for frame in ImageSequence.Iterator(imgs[1])]
    _log(f"里图帧数: {len(inner_frames)}, 大小: {imgs[0].size}, 帧延时: {imgs[0].info['duration']}ms")
    _log(f"表图帧数: {len(cover_frames)}, 大小: {imgs[1].size}, 帧延时: {imgs[1].info['duration']}ms")

    # Resize inner image to fit max_size
    for frame in inner_frames:
        frame.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)

    # Resize cover image to fit inner image
    _log(f"调整尺寸为: {inner_frames[0].size}...")
    img_ratio = cover_frames[0].width / cover_frames[0].height
    target_ratio = inner_frames[0].width / inner_frames[0].height
    if target_ratio > img_ratio:
        new_height = int(cover_frames[0].width / target_ratio)
        crop_area = (0,
                     (cover_frames[0].height - new_height) // 2,
                     cover_frames[0].width,
                     (cover_frames[0].height + new_height) // 2)
    else:
        new_width = int(cover_frames[0].height * target_ratio)
        crop_area = ((cover_frames[0].width - new_width) // 2,
                     0,
                     (cover_frames[0].width + new_width) // 2,
                     cover_frames[0].height)
    cover_frames = [frame.crop(crop_area).resize(inner_frames[0].size, Image.Resampling.LANCZOS) for frame in cover_frames]

    # Uniformize number of frames
    n_frames = min(max(len(inner_frames), len(cover_frames)), max_frames)
    inner_frames = (inner_frames * (n_frames // len(inner_frames) + 1))[:n_frames]
    cover_frames = (cover_frames * (n_frames // len(cover_frames) + 1))[:n_frames]

    _log(f"输出总帧数: {n_frames}")

    # Use the bigger duration
    duration = max(inner_frames[0].info['duration'], cover_frames[0].info['duration'])
    _log(f"输出帧延时: {duration}ms")

    return inner_frames, cover_frames, duration


def _ordered_dithering(image: Image.Image) -> Image.Image:
    '''
    使用有序抖动算法对图像进行二值化。

    :param image: PIL图像对象

    :return: PIL图像对象
    '''
    global _BAYER_MATRIX

    image = image.convert('L')
    arr = np.array(image, dtype=np.uint8)
    height, width = arr.shape

    for y in range(height):
        for x in range(width):
            threshold = _BAYER_MATRIX[y & 3, x & 3]
            arr[y, x] = 255 if arr[y, x] > threshold else 0

    return Image.fromarray(arr)


def _floyd_steinberg_dithering(image: Image.Image) -> Image.Image:
    '''
    使用Floyd-Steinberg抖动算法对图像进行二值化。

    :param image: PIL图像对象

    :return: PIL图像对象
    '''
    image = image.convert('L')
    arr = np.array(image, dtype=np.uint8)
    height, width = arr.shape

    for y in range(height):
        for x in range(width):
            old_pixel = arr[y, x]
            new_pixel = 255 if old_pixel > 127 else 0
            arr[y, x] = new_pixel
            error = old_pixel - new_pixel
            if x + 1 < width:
                arr[y, x + 1] += error * 7 // 16
            if y + 1 < height:
                if x > 0:
                    arr[y + 1, x - 1] += error * 3 // 16
                arr[y + 1, x] += error * 5 // 16
                if x + 1 < width:
                    arr[y + 1, x + 1] += error // 16

    return Image.fromarray(arr)


def _merge_img(inner_img: Image.Image, cover_img: Image.Image) -> Image.Image:
    '''
    将两个二值化图像合并为幻影坦克图。

    :param inner_img: 里图
    :param cover_img: 表图

    :return: 合并后的图像，调色板模式
    '''
    if inner_img.size != cover_img.size:
        raise ValueError("Images must have the same size")
    pixels = [inner_img.load(), cover_img.load()]
    res_img = Image.new("RGB", inner_img.size)
    pixels_res = res_img.load()
    for i in range(inner_img.size[0]):
        for j in range(inner_img.size[1]):
            # is_cover = (j // 3 + i) % 3 < 2
            # is_cover = (i + j) & 1
            # is_cover = (i + j) & 3 < 2
            is_cover = (j // 3 + i) & 3 < 2
            # is_cover = 0
            # is_cover = j & 1
            if (pixels[is_cover][i, j] != 0) == is_cover:
                l = _TRANS_COLOR
            elif is_cover:
                l = 255
            else:
                l = 0
            pixels_res[i, j] = (l, l, l)
    return res_img.convert('P', palette=Image.Palette.ADAPTIVE)


def _merge_gif(inner_frames: list, cover_frames: list) -> list:
    '''
    将两个GIF的帧列表合并为幻影坦克GIF。

    :param inner_frames: 里GIF帧列表
    :param cover_frames: 表GIF帧列表

    :return: 合并后的帧列表
    '''
    n_frames = len(inner_frames)
    if n_frames != len(cover_frames):
        raise ValueError("Number of frames must be the same")
    res_frames = []
    progress_bar = _Progress_Bar(n_frames, "合并中", "frame", False, 1)
    for i in range(n_frames):
        res_img = _merge_img(inner_frames[i], cover_frames[i])
        res_frames.append(res_img)
        # res_img.save(f"debug/frame_{i}.png")
        progress_bar.update()
    progress_bar.close()
    return res_frames


def merge(inner_path: str, cover_path: str, output_path: str, max_size: int, max_frames: int):
    '''
    读取两个GIF文件，将其合并为幻影坦克GIF并保存。

    :param inner_path: 里GIF路径
    :param cover_path: 表GIF路径
    '''

    inner_frames, cover_frames, gif_duration = _read_gif(inner_path, cover_path, max_size, max_frames)

    # Apply ordered dithering
    process_bar = _Progress_Bar(len(inner_frames) + len(cover_frames), "抖动处理中", "frame", False, 1)
    for i in range(len(inner_frames)):
        inner_frames[i] = _ordered_dithering(inner_frames[i])
        process_bar.update(1)
    for i in range(len(cover_frames)):
        cover_frames[i] = _ordered_dithering(cover_frames[i])
        process_bar.update(1)
    process_bar.close()

    # Process frames
    res_frames = _merge_gif(inner_frames, cover_frames)

    # Create custom palette and apply to frames
    custom_palette = [0, 0, 0, _TRANS_COLOR, _TRANS_COLOR, _TRANS_COLOR, 255, 255, 255] + [0] * (256 * 3 - 9)
    for frame in res_frames:
        frame.putpalette(custom_palette)

    # Find the index of the color _TRANS_COLOR in the custom palette
    transparency_index = custom_palette.index(_TRANS_COLOR) // 3

    # Save result
    res_frames[0].save(output_path, save_all=True, append_images=res_frames[1:],
                       duration=gif_duration, loop=0, transparency=transparency_index, disposal=2)
    _log(f"结果保存为: {output_path}")
