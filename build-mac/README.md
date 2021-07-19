# libetpan 4 Apple Hardware

## Prerequisites

### Package managers

MacPorts for installing dependencies:

Install [MacPorts](https://www.macports.org/) for your
[version of OS X/macOS](https://www.macports.org/install.php).

### Dependencies of prerequisites

```
sudo port install git

sudo port install gmake
sudo port install autoconf
sudo port install libtool
sudo port install automake

sudo port install wget

sudo port install gsed

xcode-select --install
```
### Dependencies

```
git clone https://gitea.pep.foundation/buff/mac-os-build-scripts-common-dependencies.git
git clone https://gitea.pep.foundation/pEp.foundation/libetpan.git
```
## Build

### Using Xcode UI

`open libetpan/build-mac/libetpan.xcodeproj`.

Build the scheme fitting your needs (libetpan [macOS|iOS].

### Using terminal

`xcodebuild -project "libetpan/build-mac/libetpan.xcodeproj" -scheme "libetpan [macOS|iOS]" -configuration [RELEASE|DEBUG]`

## Output Dir

Find arttefacts and headers in `libetpan/build-mac/build`.
