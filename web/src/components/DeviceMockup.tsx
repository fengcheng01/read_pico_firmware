import type { FC } from 'react';
interface DeviceMockupProps { imageSrc: string; altText: string; className?: string; eager?: boolean }
// 中文：示意机身包裹真实帧，不模拟面板光学。/ English: A schematic shell surrounds a real frame without simulating panel optics.
export const DeviceMockup: FC<DeviceMockupProps> = ({ imageSrc, altText, className = '', eager = false }) => (
  <div className={`device ${className}`}>
    <div className="device-power" aria-hidden="true" />
    <div className="device-screen"><img src={imageSrc} alt={altText} width="684" height="1216" loading={eager ? 'eager' : 'lazy'} /></div>
    <div className="device-keys" aria-hidden="true"><span className="key-bar" /><span className="key-round" /><span className="key-bar" /></div>
  </div>
);
