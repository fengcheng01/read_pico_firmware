import { useEffect, useState } from 'react';
import { ArrowDown, ArrowRight, BookOpen, Globe } from 'lucide-react';
import { translations, type Lang } from './utils/i18n';
import { FlasherSection } from './components/FlasherSection';
import { FirmwareDownloadSection } from './components/FirmwareDownloadSection';
import { FeaturesAndGallery } from './components/FeaturesAndGallery';
import { SpecsAndFooter } from './components/SpecsAndFooter';
import { DeviceMockup } from './components/DeviceMockup';
import home from './assets/screenshots/home.png';
export const App = () => {
  const [lang, setLang] = useState<Lang>('zh');
  const t = translations[lang];
  useEffect(() => { document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en'; }, [lang]);
  useEffect(() => {
    // 中文：React首帧之后恢复已有锚点链接。/ English: Restore direct hash links after React mounts the first frame.
    const frame = requestAnimationFrame(() => document.getElementById(window.location.hash.slice(1))?.scrollIntoView({ behavior: 'instant', block: 'start' }));
    return () => cancelAnimationFrame(frame);
  }, []);
  return <>
    <a className="skip-link" href="#main">{lang === 'zh' ? '跳到正文' : 'Skip to content'}</a>
    <header className="site-header"><div className="container nav-inner">
      <a className="wordmark" href="#top" aria-label="PageRest"><BookOpen size={23} strokeWidth={1.6} /><span>PageRest<span className="brand-dot">.</span></span></a>
      <nav aria-label={lang === 'zh' ? '主导航' : 'Main navigation'}>{['features','gallery','firmware','flasher','help'].map((id,i)=><a key={id} href={`#${id}`} className={id === 'flasher' ? 'nav-flasher' : ''}>{t.nav[i]}</a>)}</nav>
      <div className="nav-actions"><button className="language" onClick={()=>setLang(lang === 'zh' ? 'en' : 'zh')} aria-label={t.language}><Globe size={15} /><span>{t.language}</span></button></div>
    </div></header>
    <main id="main"><section className="hero container" id="top">
      <div className="hero-copy"><p className="eyebrow">{t.eyebrow}</p><h1>{t.headline[0]}<br /><span>{t.headline[1]}</span></h1><p className="lead">{t.intro}</p>
        <div className="hero-actions"><a className="button primary" href="#flasher">{t.primary}<ArrowRight size={17} /></a><a className="text-link" href="#gallery">{t.secondary}<ArrowDown size={16} /></a></div>
        <p className="compatibility">{t.compatibility}</p>
      </div>
      <div className="hero-stage"><span className="stage-edition">PAGEREST / RDP-G01-W</span><div className="hero-device"><DeviceMockup imageSrc={home} altText={`${t.device} · ${t.gallery[0].title}`} eager /></div><div className="stage-caption"><span className="status-dot" /><span>{t.heroCaption}</span></div><span className="stage-corner" aria-hidden="true">01 — 07</span></div>
    </section>
    <div className="container capability-strip">{t.strip.map((s,i)=><span key={s}><span className="mono">0{i+1}</span>{s}</span>)}</div>
    <FeaturesAndGallery lang={lang} />
    <section className="container update-band"><div><p className="eyebrow">{t.versionLabel}</p><h2>{t.updateTitle}</h2></div><div className="update-list">{t.updates.map(([title,desc],i)=><article key={title}><span className="mono">0{i+1}</span><div><h3>{title}</h3><p>{desc}</p></div></article>)}</div></section>
    <FirmwareDownloadSection lang={lang} />
    <FlasherSection lang={lang} />
    <SpecsAndFooter lang={lang} />
    </main>
  </>;
};
