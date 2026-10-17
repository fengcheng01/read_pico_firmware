# 内置图像 / Embedded images

`pagerest-boot.png` 来自用户提供的阅读人物插画，按用户要求使用内置 image_gen 去除角落水印并整理灰阶；编辑提示保存在 `pagerest-boot.prompt.txt`。它是 PageRest 的开机插画，原 `init-loading.png` 保留作历史资源。
`pagerest-boot.png` was edited from a user-provided reading illustration with built-in image_gen, removing the corner watermark and simplifying grayscale as requested. Prompts are in `pagerest-boot.prompt.txt`. It supplies PageRest's boot artwork; the original `init-loading.png` remains as a historical asset.

运行 `python3 tools/gen_boot_image.py` 将该图按比例缩放并编码为 684×1216、16 灰阶的 `main/assets/loading_4bpp.bin` 与 `.pack`。脚本依赖 Pillow，不修改原 PNG。设备一次解包、复用同一源画两帧浅至深淡入；首帧完整刷新，末帧 GL16，不添加等待或动画循环。分配或解包失败显示固件名与版本并继续原启动检查。
Run `python3 tools/gen_boot_image.py` to proportionally scale and encode the art as 684×1216, sixteen-gray `main/assets/loading_4bpp.bin` and `.pack`. The script needs Pillow and preserves the source PNG. The device inflates once and reuses that source for two light-to-full frames, first with a full refresh and then GL16, without added waits or loops. Allocation/decode failure leaves a name/version fallback and continues existing boot checks.
