# NVIDIA Optical Flow headers

The five public headers of the NVIDIA Optical Flow SDK 5.0.7 (API version
5.0), vendored because the SDK download requires an NVIDIA Developer Program
account and CI cannot fetch it anonymously. Each header carries its own MIT
notice (Copyright (c) 2020-2023 NVIDIA Corporation).

Source: <https://github.com/mbucchia/Optical-Flow-SDK> at commit
`54e68293b4898a530bc07e4d7df71efbc5d30f9b`, which mirrors the SDK's
`NvOFInterface/` directory.

The runtime `nvofapi64.dll` is a display-driver component in
`C:\Windows\System32`; it is loaded at run time and never committed or
shipped.

## License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
