# 小纸 Pico 网站 / Read Pico Website

本目录是 React、TypeScript 和 Vite 静态网站，包含产品介绍、界面截图和 Web Serial 固件烧录工具。网站通过 GitHub Actions 构建并发布到 GitHub Pages。

This directory contains the React, TypeScript and Vite static website: product information, screenshots and the Web Serial firmware flasher. GitHub Actions builds and deploys it to GitHub Pages.

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
2. 将网站文件和 `.github/workflows/pages.yml` 提交并推送到 `main`。之后修改这两个路径会自动构建并部署；也可在 Actions 中手动运行 **Build and deploy website**。
3. 工作流成功后，从部署记录打开网站。当前仓库未绑定自定义域名时的默认地址为 `https://fengcheng01.github.io/read_pico_firmware/`；实际地址以部署输出为准。

1. In **Settings → Pages → Build and deployment → Source**, select **GitHub Actions**.
2. Commit the website and `.github/workflows/pages.yml`, then push to `main`. Subsequent changes to either path trigger a build and deployment. You can also run **Build and deploy website** manually in Actions.
3. After a successful deployment, open the website from the deployment record. Without a custom domain, this repository's default URL is `https://fengcheng01.github.io/read_pico_firmware/`; use the deployment output as the authoritative URL.

拉取请求仅检查网站构建，不发布。工作流只上传 `web/dist`，不上传设备端源码、本机构建目录或交接日志。Vite 的 `base: './'` 使资源可在仓库子路径和域名根路径加载；图片通过资源导入打包。

Pull requests only validate the website build. The workflow uploads only `web/dist`, excluding device source, local build directories and handoff logs. Vite's `base: './'` supports both repository subpaths and domain roots; imported images are bundled with the website.

迁移网站托管不需要修改设备固件。原有 Cloudflare 站点可保留到 GitHub Pages 验证完成；自定义域名如需迁移，另行配置 Pages 与 DNS。

Changing the website host does not require device firmware changes. Keep the existing Cloudflare site until GitHub Pages is verified. Moving a custom domain additionally requires Pages and DNS configuration.

## 固件发布边界 / Firmware release boundary

当前烧录工具让用户选择本地 BIN 文件，不会自动下载 GitHub Release 附件。启用 Pages 只发布网站，不编译或自动发布固件。正式固件可另在 GitHub Release 管理；自动获取版本、镜像清单与下载校验需要单独实现。

The current flasher accepts locally selected BIN files and does not automatically download GitHub Release assets. Enabling Pages publishes the website without compiling or publishing firmware. Manage firmware separately in GitHub Releases; automatic version discovery, image manifests and download verification require a separate implementation.

Web Serial 烧录需要支持该 API 的桌面浏览器和 HTTPS（本地开发可用 localhost）。发布后应核对介绍页、截图和烧录区正常显示；写入校验和设备启动验收是独立于网站部署的检查。

Web Serial flashing requires a desktop browser supporting that API and HTTPS (localhost is supported for development). After deployment, check the landing page, screenshots and flasher section. Flash verification and device boot acceptance are separate from website deployment.
