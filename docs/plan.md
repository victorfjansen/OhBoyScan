# OhBoyScan Development Plan

> Development buddy planning document. Covers the overall architecture, the
> five core features, and the step-by-step roadmap. This file is the single
> source of truth for what we build next.

## 1. Project Overview

OhBoyScan is a cross-platform **Slint + C++** network security scanner for
**MikroTik routers (RouterOS v7)**, primarily targeted at a **Raspberry Pi
3B+ running Linux with a 3.5" LCD (420×320)**.

| Aspect | Decision |
| --- | --- |
| UI toolkit | Slint, defined in `.slint` files under `ui/` (entry: `ui/app-window.slint`) |
| UI library | Material Design for Slint, under `ui/material-1.0/` (import as `@material`) |
| C++ entry point | `src/main.cpp` |
| Build | CMake (`CMakeLists.txt`), binary `my_application` |
| Targets | Raspberry Pi Linux (primary) + developer desktop Linux (must build and run) |
| C++ style | Strictly object-oriented (classes, RAII, encapsulation) |
| C++ location | All C++ feature code under `src/` |
| UI constraint | Every screen fits 420×320, touch-friendly hit areas |

## 2. Current State

### Existing structure
- `ui/app-window.slint` — main window `OBMainScreen` (420×320).
- `ui/components/oh-appbar.slint` — status bar (link status + app name).
- `ui/components/oh-card-menu.slint` — feature carousel (one card per page).
- `ui/components/oh-feature-card.slint` — reusable feature card + status badge.
- `ui/globals.slint` — shared palette globals and the `Menus` enum.
- `src/main.cpp` — app entry point; includes `src/network.h` (placeholder).
- `icons/` — carousel arrows (`arrow-left`, `arrow-right`). Nav icons are no
  longer used after removing the bottom navigation bar.

### Recently completed (UI scaffolding)
- Replaced the placeholder blue rectangles with a **swipeable carousel** that
  shows exactly one feature card at a time and scrolls horizontally.
- Added **left/right arrow buttons** and **page indicator dots**.
- Made the **app bar** data-driven (`statusText`, `linkActive`) and fixed its
  height to 36px.
- Added a minimal `src/network.h` placeholder so the project compiles while
  networking classes are still pending.
- Added **click-through navigation**: tapping a card opens a full-screen
  feature view with a back button.
- Removed the bottom `NavigationBar`; the carousel is the single entry point.

## 3. Architecture

### 3.1 UI layer (`ui/`)
Layout budget for 420×320:

| Region | Component | Height |
| --- | --- | --- |
| Top status bar | `OhAppBar` | 36px |
| Content (carousel / feature views) | `OhCardMenu` / `OhFeatureDetail` | fills remaining (284px) |

- The carousel uses `SwipeGestureHandler` for touch/mouse swipe plus arrow
  buttons; `current_index` drives an animated page track.
- Feature cards use Material `OutlinedCard` with a colored status badge
  (`FeatureStatus`: `neutral`, `ok`, `warn`, `error`).

### 3.2 C++ layer (`src/`)
Object-oriented modules (one class per responsibility):

| Class | Responsibility | Status |
| --- | --- | --- |
| `RouterOsApiClient` | RouterOS v7 REST API over HTTPS (libcurl, basic auth) | done |
| `MikrotikDevice` | Model a target router (identity, model, IP, credentials, state) | done |
| `ScanEngine` | Owns the worker thread; runs scans off the UI thread; reports results | done |
| `GatewayDetector` | Auto-detect the default IPv4 gateway from `/proc/net/route` | done |
| `EcdsaValidator` | Feature 1 — ECDSA P-256 verify + self-test (OpenSSL) | done |
| `TopologyScanner` | Feature 2 — MNDP/LLDP discovery (AF_PACKET raw sockets) | done |
| `ArpMonitor` | Feature 3 — continuous ARP spoofing watch (AF_PACKET) | done |
| `CableTester` | Feature 4 — TDR cable test via REST | done |
| `ExposureChecker` | Feature 5 — scans exposed management ports (TCP) | done |

Rules:
- RAII for sockets/threads/buffers, no raw `new`/`delete` without owners.
- No C-style procedural code; everything encapsulated in classes.
- Cross-platform: standard POSIX sockets and Linux raw `AF_PACKET` sockets so
  desktop Linux and Raspberry Pi Linux behave the same.

### 3.3 Slint ↔ C++ binding
- C++ exposes data as Slint properties/models and reacts to callbacks.
- Scans run on worker threads; results are marshalled back to the UI thread
  (e.g., via `slint::invoke_from_event_loop`).

### 3.4 Screen navigation
- Main screen = carousel (one card per page). Tapping a card opens that
  feature's screen. There is no bottom navigation bar; the carousel is the
  single entry point.
- The feature screen is full-screen with a back button in its top bar;
  tapping back returns to the main carousel.
- No user input: opening a feature screen auto-starts its scan and only shows
  status/results.
- Scan mode is per feature: one-shot for audits/tests (ECDSA, cable test,
  exposure), continuous for the ARP monitor.
- `OBMainScreen` owns `features` (model) and `current-feature` (index, -1 =
  main). `OhCardMenu` emits `feature-selected(index)`; `OhFeatureDetail`
  emits `back-requested()`.

## 4. The Five Features

### Feature 1 — ECDSA Script Integrity Audit
- **What**: validate RouterOS `.rsc` script signatures (FIPS 186-5 ECDSA) via
  the API and show a badge: green `[VALID]` or red `[TAMPERED]`.
- **UI**: `OhEcdsaView` shows the crypto-engine self-test and REST API status.
- **C++**: `EcdsaValidator` (ECDSA P-256 + SHA-256 verify and self-test via
  OpenSSL).
- **420×320**: compact check rows with color-coded status.
- **Implemented**: OpenSSL ECDSA verify + self-test, and a REST `system/script`
  probe against the detected gateway. A signature registry for per-script
  `[VALID]/[TAMPERED]` verdicts is a follow-up.

### Feature 2 — Topology / Neighbor Mapper (MNDP / LLDP / CDP)
- **What**: capture L2 discovery packets and instantly show the router
  Identity, RouterBOARD model, RouterOS version, management IP, and physical
  port.
- **UI**: `OhTopologyView` lists discovered neighbors with identity, model,
  version, IP, and protocol.
- **C++**: `TopologyScanner` + MNDP/LLDP parsers over raw `AF_PACKET` sockets.
- **420×320**: compact two-line rows in a vertical scroll area.
- **Implemented**: passive capture of MNDP (UDP 5678) and LLDP (0x88CC) on the
  gateway-facing interface. CDP parsing is a follow-up.

### Feature 3 — ARP Cache Poison Monitor (Anti-MitM)
- **What**: listen on L2 (raw `AF_PACKET`) and alert in real time when the
  gateway MAC is forged/duplicated (ARP spoofing/poisoning).
- **UI**: `OhArpView` shows a live status line and a scrolling alert feed.
- **C++**: `ArpMonitor` (capture thread, ARP parsing, baseline MAC diffing,
  event callback).
- **420×320**: status line + compact colored alert rows.
- **Implemented**: continuous ARP capture on the gateway-facing interface;
  learns the first gateway MAC as baseline and alerts on any change. Needs
  root/CAP_NET_RAW.

### Feature 4 — Cable Test / TDR
- **What**: trigger `/interface ethernet cable-test` via API and show whether
  the cable is broken or shorted plus the exact distance in meters.
- **UI**: `OhCableView` lists each interface with status and distance.
- **C++**: `CableTester` (REST command + JSON response parsing).
- **420×320**: compact rows, color-coded status, distance on the right.
- **Implemented**: fetches ethernet interfaces and POSTs `cable-test` for
  each, parsing `status`/`distance`. Field names need validation on a real
  RouterOS device.

### Feature 5 — Insecure Management Interface Checker
- **What**: quick scans of critical ports (WebFig HTTP 80, Winbox 8291,
  API 8728, Telnet/SSH) to detect default credentials or missing
  `allowed-address` restrictions on the WAN.
- **UI**: Exposure Checker view lists each port with OPEN/CLOSED and a color.
- **C++**: `ExposureChecker` (TCP port scan now; credential/ACL checks later).
- **420×320**: compact per-port rows with color-coded status.
- **Implemented**: TCP scan of WebFig HTTP 80, WebFig SSL 443, Winbox 8291,
  API 8728, SSH 22, Telnet 23 against the detected gateway, running
  automatically on open.

## 5. UI Roadmap

1. **Done** — Carousel with feature cards, app bar, arrows, dots.
2. **Done** — Click-through navigation: tap a card → full-screen feature view
   with a back button; auto-scan placeholder shown.
3. Add per-feature **detail content** (lists/tables/status) as each feature is
   implemented.
4. Add feature-specific SVG icons.
5. Polish for touch: 40px+ hit targets, no 420×320 overflow, dark scheme.

## 6. Implementation Roadmap

| Phase | Work | Status |
| --- | --- | --- |
| 0 | Project scaffolding (CMake, Slint, Material) | done |
| 1 | Carousel UI + feature cards + app bar | done |
| 1b | Click-through navigation + feature detail screens | done |
| 2 | C++ core: `MikrotikDevice`, `RouterOsApiClient`, `ScanEngine`, `GatewayDetector` | done |
| 3 | Feature 5 (TCP port scan) — first end-to-end | done |
| 4 | Feature 2 (topology mapper, MNDP/LLDP) | done |
| 5 | Feature 1 (ECDSA validation) | done |
| 6 | Feature 3 (ARP monitor) | done |
| 7 | Feature 4 (cable test) | done |
| 8 | Per-feature results views | next |
| 9 | Raspberry Pi bring-up on the 420×320 LCD | planned |

Ordering rationale: start with the feature that exercises the full
C++ ↔ RouterOS ↔ UI pipeline (Feature 5), then grow from there.

## 7. Verification

- Build: `cmake --build build`
- Run: `./build/my_application` (desktop Linux and Raspberry Pi)
- Check: no UI overflow at 420×320, touch hit areas ≥40px, dark scheme
  readable on the LCD.
- Cross-platform: dependencies are libcurl, OpenSSL, and standard POSIX +
  Linux `AF_PACKET` sockets, all available on desktop Linux and Raspberry Pi
  Linux.

## 8. Open Items

- Cable test REST response field names (`status`/`distance`) need validation
  against a real RouterOS device.
- ARP monitor learns the first-seen gateway MAC as baseline; a trusted
  baseline (config/REST cross-check) is a follow-up.
- ECDSA: a per-script signature registry (for `[VALID]/[TAMPERED]` verdicts)
  is still a follow-up; the crypto engine and REST probe are done.
- Topology capture needs root/CAP_NET_RAW; CDP parsing is a follow-up (MNDP +
  LLDP are implemented); raw `AF_PACKET` sockets are used instead of libpcap.
- Exposure Checker performs a TCP port scan only; default-credential and
  `allowed-address` (ACL) checks are follow-ups.
- Feature detail screens are auto-scan placeholders until the per-feature C++
  classes land.
- Feature icons still missing; reuse Material icons or add new SVGs.
- `RouterOsApiClient` trusts self-signed TLS certs (field-friendly); revisit
  certificate pinning before a hardened release.
- Confirmed: no bottom navigation bar; full-screen feature views; scan
  auto-runs on open (one-shot except the continuous ARP monitor); target
  router auto-detected from the default gateway.
- Transport decided: RouterOS v7 REST API over HTTPS via libcurl (basic auth).
