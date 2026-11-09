# Third-party notices

MyKam itself is MIT licensed (see `LICENSE`). It includes or uses the components below. Each keeps its own
licence. The licence texts are in the repository under `third_party/` and in the release zip under `licences/`.

| Component | Version | Licence | Source | Licence file in the repository | Licence file in the release zip |
|-----------|---------|---------|--------|--------------------------------|---------------------------------|
| X-Plane SDK (XPLM headers and Windows import libraries) | 4.3.0 | BSD-style licence of Laminar Research (see below) | https://developer.x-plane.com/sdk/ | `third_party/XPSDK/license.txt` | `licences/X-Plane-SDK-license.txt` |
| Dear ImGui | 1.91.5 | MIT | https://github.com/ocornut/imgui (tag v1.91.5) | `third_party/imgui/LICENSE.txt` | `licences/Dear-ImGui-LICENSE.txt` |
| doctest (unit tests only, not in the release zip binary) | 2.4.11 | MIT | https://github.com/doctest/doctest (release 2.4.11) | `third_party/doctest/LICENSE.txt` | `licences/doctest-LICENSE.txt` |
| Inter (UI text font) | 4.0 | SIL Open Font License 1.1 | https://github.com/rsms/inter (release v4.0) | `third_party/fonts/inter/OFL.txt` | `licences/Inter-OFL.txt` (also `fonts/OFL-Inter.txt`) |
| Material Icons (UI icon font) | google/material-design-icons commit 10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7 | Apache License 2.0 | https://github.com/google/material-design-icons | `third_party/fonts/material/LICENSE` | `licences/MaterialIcons-LICENSE.txt` (also `fonts/LICENSE-MaterialIcons.txt`) |

Exact versions, download sources and notes are also kept in `third_party/VERSIONS.md`.

## X-Plane SDK

MyKam is built against the X-Plane SDK headers and import libraries. The SDK licence reads:

```
Copyright (c) 2008, Sandy Barbour and Ben Supnik
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Neither the names of the authors nor that of X-Plane or Laminar Research
      may be used to endorse or promote products derived from this software
      without specific prior written permission from the authors or
      Laminar Research, respectively.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

The names X-Plane and Laminar Research are not used to endorse or promote MyKam. MyKam is an independent
project and is not made, approved or supported by Laminar Research.

## Dear ImGui

Copyright (c) 2014-2024 Omar Cornut. MIT licence. Compiled into `MyKam.xpl`. Full text: `third_party/imgui/LICENSE.txt`.

## doctest

Copyright (c) 2016-2023 Viktor Kirilov. MIT licence. Used only to build the unit tests; it is not part of `MyKam.xpl`.
The doctest source ships its licence reference in the header (`doctest.h`); `third_party/doctest/LICENSE.txt` holds the MIT text.

## Inter

Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter). SIL Open Font License, Version 1.1.
`Inter-Regular.ttf` and `Inter-SemiBold.ttf` are distributed unchanged in the release zip under `fonts/`.
Full text: `third_party/fonts/inter/OFL.txt`.

## Material Icons

Material Icons by Google, Apache License 2.0. `MaterialIcons-Regular.ttf` is distributed unchanged in the release
zip under `fonts/`. Full text: `third_party/fonts/material/LICENSE`.
