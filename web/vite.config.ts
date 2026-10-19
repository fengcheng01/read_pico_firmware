import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react(), tailwindcss()],
  base: './',
  build: {
    rolldownOptions: {
      output: {
        // 烧录期间不得再联网加载芯片辅助程序；首次页面加载时一次取得完整脚本。
        // Flashing must not fetch chip helpers mid-session; load one complete script with the page.
        codeSplitting: false,
      },
    },
  },
  server: {
    port: 3000,
  },
});
