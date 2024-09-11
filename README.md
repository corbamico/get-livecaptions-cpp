# get-livecaptions-cpp
Get real time content of Windows System APP "Live Captions" [win+ctrl+L], write content into file. using c++/winrt, asio

check slibing project [get-livecaptions-rs](https://github.com/corbamico/get-livecaptions-rs)

## Usage

```cmd
Optional arguments:
  -h, --help         shows help message and exits
  -v, --version      prints version information and exits
  -o, --output file  filename, write content into file. use - for console. [required]
```

## UIAutomation

To find the LiveCaptions GUI AutomationID, you may need tools as [inspect](https://learn.microsoft.com/en-us/windows/win32/winauto/inspect-objects), or [Automation-Spy](https://github.com/ddeltasolutions/Automation-Spy)

![](./doc/image.png)

## License

Licensed under either of

 * Apache License, Version 2.0
   ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license
   ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.

