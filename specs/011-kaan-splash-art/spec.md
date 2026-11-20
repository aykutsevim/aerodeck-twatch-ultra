# Feature Specification: KAAN Splash Screen and Watch-Face Emblem

**Feature Branch**: `011-kaan-splash-art`

**Created**: 2026-11-17

**Status**: Draft

**Input**: User description: "add a initial page and put Turkish Kaan silhouette art from art directory
kaan-splash-screen.jpg, also put smaller version of this image to PFD watch fate page, bottom middle between zulu and
current time"

**Traceability**: AD-SRS-001 FR-PLT-001 (boot to PFD ≤ 4 s), NFR-REL-001 / constitution (always boot to a usable
PFD) · PFD layout (bottom-left ZULU, bottom-right LOCAL corners) · Source art: `art/kaan-splash-screen.jpg`
(1696 × 2528 px, white top-view KAAN fighter silhouette with the word "KAAN", on black), provided by the wearer.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - KAAN splash screen at start-up (Priority: P1)

When the watch starts, the wearer sees a full-screen splash showing the KAAN silhouette art (aircraft and "KAAN"
lettering, white on black) while the watch gets ready, and then the PFD watch face appears as before.

**Why this priority**: It is the main request: a branded "initial page" that gives the watch its identity every time
it starts.

**Independent Test**: Restart the watch (power-on or `ad reboot`). The splash appears, fills the screen without
distortion, and is followed by the PFD within the existing boot-time limit.

**Acceptance Scenarios**:

1. **Given** the watch is off or restarting, **When** it starts, **Then** the KAAN art is shown full-screen, centred,
   with its proportions preserved, on a black background, before the watch face.
2. **Given** the splash is showing, **When** start-up completes, **Then** the PFD watch face replaces it, and the total
   time from power-on to a usable PFD stays within the 4 s requirement.
3. **Given** the splash is showing, **When** the wearer taps or presses the button, **Then** nothing breaks: the PFD
   still appears as soon as start-up is done (the splash cannot be used to block or delay the watch).
4. **Given** an optional peripheral fails at start-up, **When** the watch starts, **Then** the splash still gives way to
   a usable PFD with the usual CAUTION, never a hang on the splash.

---

### User Story 2 - KAAN emblem on the PFD watch face (Priority: P2)

On the PFD watch face, a small version of the same KAAN art sits at the bottom centre, between the ZULU time
(bottom-left) and the LOCAL time (bottom-right).

**Why this priority**: A permanent, subtle emblem on the face the wearer sees most; secondary to the splash.

**Independent Test**: Open the PFD. The emblem is visible at the bottom centre, recognisable as the KAAN silhouette,
and overlaps none of the existing elements (ZULU, LOCAL, horizon, tapes, status values).

**Acceptance Scenarios**:

1. **Given** the PFD is shown, **When** the wearer looks at the bottom of the face, **Then** a small KAAN emblem is
   centred horizontally between the ZULU and LOCAL times, at the same height band.
2. **Given** the emblem is shown, **Then** it does not cover or clip any existing watch-face element, and all existing
   values stay fully readable.
3. **Given** NVG night mode is on, **When** the PFD is shown, **Then** the emblem follows the night colours like the
   rest of the face (no bright white spot).
4. **Given** the PFD is animating (horizon, tapes), **Then** the watch face stays as smooth as before (no visible drop
   in frame rate).

---

### User Story 3 - KAAN page in the page ring (Priority: P3) *(added during implementation, user request)*

The large KAAN art is also a page of the app: the last page of the ring, after Settings, so a swipe right on the
watch face shows it. The watch still starts on the watch face (after the splash).

**Acceptance Scenarios**:

1. **Given** the watch face, **When** the wearer swipes right, **Then** the KAAN page shows the large KAAN art.
2. **Given** the KAAN page, **When** the wearer swipes left, **Then** the watch face returns; from Settings a
   swipe left leads to the KAAN page.
3. **Given** a full start, **Then** the app lands on the watch face (not on the KAAN page).

---

### Edge Cases

- **Wake from eject (deep sleep):** the splash is **not** shown; waking goes straight to the watch face so the S7
  wake-to-time behaviour stays fast. The splash is for a full start (power-on, reset, restart, firmware update).
- **Safe mode / repeated crash at start-up:** the splash must not add to a boot loop; if start-up falls back to safe
  mode, the splash is skipped or ends normally.
- **Screen shape:** the art is portrait (≈ 2 : 3); the screen is 410 × 502 with rounded corners. The art is scaled to
  fit inside the screen (no cropping of the aircraft or the lettering by the corners), with black around it.
- **Narrow gap on the watch face:** if the space between ZULU and LOCAL is small, the emblem is scaled down to fit that
  space rather than moving other elements.
- **Missing or damaged art data:** the watch still starts and shows the PFD (no emblem / plain black splash).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The watch MUST show a full-screen splash with the KAAN silhouette art at every full start (power-on,
  reset, restart, firmware update), before the PFD watch face.
- **FR-002**: The splash art MUST be the wearer's `kaan-splash-screen.jpg` (aircraft and "KAAN" lettering), scaled to
  fit the screen with its proportions preserved, centred, on black, and not clipped by the rounded corners. The plain
  black margins around the artwork may be trimmed; the artwork itself is not changed.
- **FR-003**: The splash MUST NOT extend the time to a usable PFD beyond the existing 4 s boot requirement
  (FR-PLT-001); it is shown while start-up proceeds, not as an extra wait.
- **FR-004**: The splash MUST NOT appear when waking from eject (deep sleep).
- **FR-005**: The watch MUST reach a usable PFD even if the splash cannot be shown or a peripheral fails (no hang on the
  splash; constitution: always boot to a usable PFD).
- **FR-006**: The PFD watch face MUST show a small version of the same art at the bottom centre, horizontally centred
  between the ZULU (bottom-left) and LOCAL (bottom-right) times, in the same height band.
- **FR-007**: The emblem MUST NOT overlap or clip any existing watch-face element, and all existing values MUST remain
  readable.
- **FR-008**: In NVG night mode the emblem (and the splash, if shown at night) MUST follow the night colour scheme like
  the rest of the screen.
- **FR-009**: Adding the emblem MUST NOT reduce the watch face's smoothness below the existing target (median ≥ 30 fps).
- **FR-010**: The art MUST be built into the firmware (no SD card or network needed to show it).
- **FR-011** *(added, US3)*: The app MUST have a KAAN page showing the large art, placed last in the page ring
  (after Settings, i.e. a swipe right from the watch face). The start page stays the watch face. It follows NVG.

### Key Entities

- **Splash art**: the full-screen version of the KAAN image, prepared from the source file for the watch screen
  (black background, white silhouette).
- **Watch-face emblem**: a small version of the same image sized for the gap between ZULU and LOCAL.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In 5 of 5 full starts, the KAAN splash is visible before the watch face, and the watch face is usable
  within 4 s of power-on (same limit as today).
- **SC-002**: In 3 of 3 wakes from eject, no splash is shown and the time appears as fast as before this change: the mean
  wake-to-watch-face time of 3 wakes is within 50 ms of the value measured before the change.
- **SC-003**: The wearer confirms the splash looks right: the whole aircraft and the "KAAN" lettering are visible,
  undistorted, and not cut by the screen corners.
- **SC-004**: The wearer confirms the watch-face emblem is recognisable as the KAAN silhouette, centred between ZULU and
  LOCAL, and overlaps nothing.
- **SC-005**: With the emblem in place, the watch face keeps a median of ≥ 30 fps during horizon motion.
- **SC-006**: In NVG mode the emblem shows in night colours (wearer check), with no bright white area.

## Assumptions

- The wearer owns or is entitled to use the provided artwork for this personal watch; it is used as given (no
  redesign). The image is a stylised silhouette, not an official logo.
- "Initial page" means a start-up splash screen, not a new page in the swipe ring. It is not revisitable by swiping.
- The splash is shown only for full starts; wake from eject goes straight to the watch face (keeps S7/S8 wake timing).
- The splash is visible from the moment the screen hardware is ready (partway through start-up) until the first
  watch-face frame. That window is measured during validation. There is no fixed minimum display time and no hold
  (FR-003). No setting to turn it off in this slice.
- The emblem is purely decorative: not tappable, no function.
- The art is converted at build time to the watch's display format; the source JPEG is kept in `art/` and the converted
  data in the firmware. Flash space is ample (≈ 50 % free).
- The T-Watch S3 only has to keep compiling: it gets neither the splash nor the emblem in this slice (its square
  layout has no gap between the time corners). The Ultra is the reference device.
