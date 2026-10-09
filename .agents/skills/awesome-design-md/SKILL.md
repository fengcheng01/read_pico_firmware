---
name: awesome-design-md
description: Apply and reference professional brand DESIGN.md design systems from VoltAgent/awesome-design-md (Apple, Stripe, Linear, Vercel, Claude, Notion, Figma, etc.) to generate brand-aligned, production-ready frontend UI and components without AI aesthetic drift.
---

# awesome-design-md

A comprehensive collection of `DESIGN.md` design system specifications extracted and reverse-engineered from world-class tech companies and products, curated by VoltAgent.

## What is DESIGN.md?

A `DESIGN.md` file acts as the visual source of truth for AI coding agents (the visual counterpart to `AGENTS.md`):
- Semantic color palettes and functional roles
- Exact typography hierarchies and scaling
- Spacing rules, layout grids, and padding scales
- Component states (buttons, cards, inputs, tags, dialogs)
- Elevation, shadows, borders, and border-radii
- Explicit Do's and Don'ts to prevent generic "AI slop"

## Available Brand Design Systems

The complete collection of brand design systems is stored in `./design-md/`. Available profiles include:

- **AI & Developer Platforms**: `claude`, `linear.app`, `cursor`, `vercel`, `figma`, `framer`, `supabase`, `replit`, `cohere`, `elevenlabs`, `expo`, `clickhouse`, `hashicorp`
- **Fintech & Enterprise**: `stripe`, `coinbase`, `binance`, `kraken`, `mastercard`, `ibm`, `intercom`
- **Consumer & Tech Giants**: `apple`, `airbnb`, `spotify`, `notion`, `tesla`, `nike`, `uber`, `rayban`
- **Luxury & Automotive**: `porsche`, `ferrari`, `bmw`, `bmw-m`, `bugatti`, `lamborghini`

## How to Use This Skill

1. **Inspect or Choose a Brand Profile**:
   When requested or designing a UI, read the relevant brand's specification:
   ```
   read .agents/skills/awesome-design-md/design-md/<brand>/DESIGN.md
   ```
   For example:
   - For minimalist dark mode with precision borders: check `linear.app` or `cursor`.
   - For warm cream editorial typography: check `claude`.
   - For high-contrast developer tools: check `vercel` or `supabase`.
   - For clean, airy, high-end consumer UI: check `apple` or `airbnb`.

2. **Scaffold or Refactor UI Components**:
   Extract and strictly follow the design tokens (colors, font sizes, margins, border radii) from the chosen `DESIGN.md` when building React/HTML/Tailwind components.
