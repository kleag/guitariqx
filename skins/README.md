# Built-in skin

The gx_head "Guitarix" skin, built into guitariqx so that the gx_head look
is available on computers without guitarix. The other gx_head skins are
read from an installed guitarix (`gx_head/skins` in the XDG data
directories, e.g. `/usr/share/gx_head/skins`).

- `gx_head_Guitarix.css`: the compiled skin style sheet installed by
  guitarix (`rcstyles/gx_head_Guitarix.scss` in the guitarix sources);
  the other skins `@import` it.
- `gxw/`: images of `libgxw/icons` in the guitarix sources (knobs,
  backgrounds, rack handles and screws, switch icons). gx_head compiles them
  into libgxw; a skin directory overrides some of them.

The images are copied from `libgxw/icons` of the guitarix sources
(https://github.com/brummer10/guitarix, commit 4cb59f56), the style sheet
from `/usr/share/gx_head/skins` of a guitarix 0.47 installation built
from those sources. Copyright (C) Hermann Meyer, James Warden, Andreas
Degert and the guitarix contributors, GNU General Public License version 2
or later (libgxw source headers, debian/copyright of guitarix).
