# PageRest 网站 / PageRest Website

本目录是 React、TypeScript 和 Vite 静态网站，包含产品介绍、界面截图和 Web Serial 固件烧录工具。网站可通过既有Cloudflare Pages的read-pico项目随main自动部署；GitHub Pages工作流保留。

This directory contains the React, TypeScript and Vite static website: product information, screenshots and the Web Serial firmware flasher. The existing Cloudflare Pages read-pico project deploys main automatically; the GitHub Pages workflow remains available.

## 本地开发 / Local development

需要 Node.js 24；在 `web/` 目录执行：

Use Node.js 24 and run these commands inside `web/`:

```sh
npm ci
npm run dev
```

生产构建与本地预览：

Build and preview the production website locally:

```sh
npm run build
npm run preview
```

## GitHub Pages 发布 / GitHub Pages deployment

1. 在仓库 **Settings → Pages → Build and deployment → Source** 选择 **GitHub Actions**。
2. 将网站文件和 `.github/workflows/pages.yml` 提交并推送到 `main`。之后修改网站、工作流或web/firmware-version.txt会自动构建并部署；也可在 Actions 中手动运行 **Build and deploy website**。
3. 工作流成功后，从部署记录打开网站。当前仓库未绑定自定义域名时的默认地址为 `https://fengcheng01.github.io/read_pico_firmware/`；实际地址以部署输出为准。

1. In **Settings → Pages → Build and deployment → Source**, select **GitHub Actions**.
2. Commit the website and `.github/workflows/pages.yml`, then push to `main`. Subsequent website/workflow/firmware-version.txt changes trigger a build and deployment. You can also run **Build and deploy website** manually in Actions.
3. After a successful deployment, open the website from the deployment record. Without a custom domain, this repository's default URL is `https://fengcheng01.github.io/read_pico_firmware/`; use the deployment output as the authoritative URL.

拉取请求仅检查网站构建，不发布。工作流只上传 `web/dist`，不上传设备端源码、本机构建目录或交接日志。Vite 的 `base: './'` 使资源可在仓库子路径和域名根路径加载；图片通过资源导入打包。

Pull requests only validate the website build. The workflow uploads only `web/dist`, excluding device source, local build directories and handoff logs. Vite's `base: './'` supports both repository subpaths and domain roots; imported images are bundled with the website.

迁移网站托管不需要修改设备固件。原有 Cloudflare 站点可保留到 GitHub Pages 验证完成；自定义域名如需迁移，另行配置 Pages 与 DNS。

Changing the website host does not require device firmware changes. Keep the existing Cloudflare site until GitHub Pages is verified. Moving a custom domain additionally requires Pages and DNS configuration.

## 固件发布边界 / Firmware release boundary

当前官网在“下载固件”区提供公开版本的 raw full / incremental BIN、manifest 与 SHA256SUMS；烧录工具仍让用户选择本地 BIN，不会在浏览器内自动下载或静默更新设备。启用 Pages 只发布网站，不编译固件；版本镜像由同次产品构建生成后放入对应的 `web/public/firmware/<version>/`。自动生成版本、上传镜像与发布清单仍需要单独的 CI 流程。

The website now exposes the selected release's raw full / incremental BIN, manifest and SHA256SUMS under its Download firmware section. The flasher still accepts a locally selected BIN and never silently downloads or updates a device. Pages publishes the website but does not compile firmware; same-build images are placed under `web/public/firmware/<version>/` before deployment. Automatic build, upload and release-manifest generation still require a separate CI flow.

Web Serial 烧录需要支持该 API 的桌面浏览器和 HTTPS（本地开发可用 localhost）。发布后应核对介绍页、截图和烧录区正常显示；写入校验和设备启动验收是独立于网站部署的检查。

Web Serial flashing requires a desktop browser supporting that API and HTTPS (localhost is supported for development). After deployment, check the landing page, screenshots and flasher section. Flash verification and device boot acceptance are separate from website deployment.

## BIN 导入与校验 / BIN import and validation

在网站地址添加 `?tool=flasher#flasher` 可直接进入独立刷机页。
Append `?tool=flasher#flasher` to the site URL to open only the flasher.

烧录合并成一个本地BIN入口，按内容识别而非文件名：incremental应用写0x10000，full完整镜像原样写0x0。没有A/B切换、full自动提取应用或三件套分支。选择后立即显示镜像版本、大小、写入地址与数据影响；结构/长度/XOR/芯片/应用描述/分区容量失败时禁止开始。工具不读取设备分区，增量需用户确认现有PageRest布局匹配；首次安装用完整包。
One local BIN entry detects content rather than names: apps write at0x10000 and whole full images at0x0. No A/B tabs, full-to-app extraction or three-file branch. Selection immediately shows version, size, address and data impact; invalid structure/length/XOR/chip/app descriptor/capacity blocks starting. The tool does not read device partitions; app upgrades require a matching PageRest layout, while first installs use full images.

完整包包含设置区空洞填充，即使全片擦除关闭也重置设置与阅读记录。高级选项只在full包显示额外全片擦除，默认关闭；该选项会进一步删除内置书库等存储。选择增量包自动清除此选项，计划生成器拒绝增量+全擦。确认对话框再次显示实际文件/地址和影响，取消不打开串口。
Full images include settings-region padding and reset settings/reading records even with full erase OFF. Advanced options offer additional full erase only for full images, OFF by default; this also deletes internal books/storage. App selection clears erase, and the planner rejects app-plus-erase. Confirmation repeats the actual file/address/data impact; cancellation never opens serial.

读文件失败/格式失败清空旧选择；异步读取代次丢弃旧文件迟到结果，忙碌时禁止替换或重复开始。结构校验不代替发行包SHA256或设备启动验收。
Read/format failures clear old selections; generations discard late results from old reads, and busy guards block replacement/double start. Structural checks do not replace release SHA256 or device boot acceptance.

先构建固件，再在 `web/` 执行 `npm run test:firmware`（Node24），使用实际full/app镜像验证导入、相同升级负载、统一入口内容识别/地址/增量全擦拒绝、损坏与截断。选择文件/校验不访问串口；开始烧录才请求设备。外部网站上的“An unknown system error has occurred”仍须结合具体网址和发生阶段排查，不能仅凭字样断定BIN损坏。
Build firmware first, then run `npm run test:firmware` in `web/` (Node24) against actual full/app images, covering import, identical upgrade payloads, unified content/address planning, app erase rejection, corruption and truncation. Selection/validation does not access serial; flashing requests a device. Diagnosing that generic error on another site still requires the URL and failure stage; the message alone does not establish a damaged BIN.

刷机错误保留选择串口/握手/写入阶段与原始异常名，取消不再重复弹窗，部分写入失败不自动重试。退出烧录协议的应答不能证明重启；应答或失败后均发送DTR=false、RTS=true等待200ms、RTS=false等待200ms的板级复位脉冲。信号执行失败显示复位未确认和手动重启提示，脉冲执行完成也仍须查看设备启动及版本。对应测试：`npm run test:flasher`（模拟边界，不访问设备）。线上发布需另行部署本构建，修改本地文件不会更新网站。
Flashing errors retain the chooser/handshake/write stage and original exception name. Cancellation does not reopen the chooser and partial writes are not retried. A flash-protocol ACK does not establish reboot. After either ACK or failure, execute the board reset: DTR=false, RTS=true for200ms, then RTS=false for200ms. Signal failure reports unconfirmed reset/manual restart; even a completed pulse still requires device boot/version confirmation. Run `npm run test:flasher` with mocked boundaries and no device access. Local edits require a separate deployment to update the public site.


## 首页与截图 / Homepage and screen assets

PageRest为固件名；当前适配设备为小纸Pico/Read Pico RDP-G01-W，不宣称已经适配其他品牌。首页版本从web/firmware-version.txt导入，不手写旧版本号；功能、图库、统一烧录、帮助和硬件摘要使用中英双语。仓库暂不公开，公开页面不提供本项目GitHub、源码或发行页入口；src/site.ts仅导出发布版本。第三方许可中的必要出处保留。
PageRest names the firmware; the supported device is Read Pico RDP-G01-W, without claiming existing ports to other brands. Import the version from web/firmware-version.txt. Features, gallery, unified flasher, help and hardware summary are bilingual. Public pages temporarily omit this project’s GitHub, source and release links while the repository remains private; src/site.ts exports only the version. Retain necessary third-party license attribution.

src/assets/screenshots与对应public/screenshots为原生684×1216帧导出，使用示例书库、模拟无线与系统边界，不是真机光学照片。官网可切换正在读/六封面书架/正文/手帐/摘录锁屏/传输/蓝牙，不把宿主画面当作无残影、无闪或型号兼容保证。设备外框是CSS示意，沿用真实帧比例。支持键盘图库切换、原生对话框焦点/取消、手机布局及减少动画偏好。
Screen assets are native684×1216 exports with sample books and mocked radio/system boundaries, not optical device photos. Gallery includes home/shelf/reader/journal/quote lock/transfer/BLE without promising ghost-free, flash-free or model compatibility. The CSS shell preserves screen proportions. Support keyboard gallery navigation, native dialog focus/cancel, responsive layout and reduced motion.

在web执行npm run test:flasher-ui运行真实React组件逻辑，替换hook调度、FileReader和串口边界，验证full/app真实文件、确认/取消、迟到读取、损坏/读取失败、换文件清除全擦与忙碌/重复开始。它不替代浏览器CSS/焦点或实机写入验收。
Run npm run test:flasher-ui in web to exercise real component logic with hook scheduling, FileReader and serial boundaries mocked. Check actual full/app plans, confirmation/cancel, stale reads, read/corruption failures, erase reset and busy/double-start guards. It does not replace browser CSS/focus or hardware write acceptance.

发布静态文件保留public/THIRD_PARTY_LICENSES.txt内的依赖许可；升级已打包依赖版本时同步更新该文件。
Preserve public/THIRD_PARTY_LICENSES.txt in static distribution; refresh it when upgrading bundled dependency versions.


## 当前生产站点 / Current production site

Cloudflare Pages项目`read-pico`连接`fengcheng01/read_pico_firmware`的`main`，根目录`web`、构建`npm run build`、输出`dist`，自定义域`pico.gofree.kdns.fr`。本地提交或功能分支打包不会更新生产；必须推送官网改动到该生产分支并核对Cloudflare部署成功。`web/.node-version`固定Node24。不将固件源码、source.zip、本机日志或私有文件放入`dist`。
Cloudflare Pages project`read-pico` tracks`main` in`fengcheng01/read_pico_firmware`, with root`web`, command`npm run build`, output`dist` and domain`pico.gofree.kdns.fr`. A local commit or feature-branch build does not update production; push website changes to the production branch and verify the Cloudflare deployment. `web/.node-version` pins Node24. Never put firmware source, source.zip, local logs or private files in`dist`.


官网发布版本使用`web/firmware-version.txt`，不为发布网页修改设备根目录`version.txt`，避免官网更新误触发固件编译。设备镜像仍以根目录版本为准；发版前运行`npm run test:firmware`核对官网元数据与公开下载目录中的同次构建 BIN。固件工作流继续检查真实源码、配置、设备版本等改动，不能用禁用检查掩盖缺失依赖。
Website release identity comes from`web/firmware-version.txt`; do not change device-root`version.txt` just to publish a site. Device images retain the root version. Before release run`npm run test:firmware` to compare website metadata with the same-build BIN pair in the public download directory. Firmware CI still checks actual source/config/device-version changes; disabling checks is not a substitute for complete dependencies.
