# MoHSES - TCP Bridge
The TCP Bridge is a simple TCP socket server which allows for implementing MoHSES modules without the need for the entire DDS middleware stack. It is not a complete implementation of MoHSES but supports the most common functionality used in module development. The TCP Bridge serves as an excellent testbed for prototyping and rapid development.

- Stateful
- Persistent
- Uses simple TCP sockets.
- Supports wired and wireless module connections.

#### Requirements
- [MoHSES Standard Library](https://github.com/AdvancedModularManikin/amm-library) (and FastRTPS and FastCDR)
- tinyxml2 (`apt-get install libtinyxml2-dev`)

### Installation
```bash
    $ git clone https://github.com/AdvancedModularManikin/tcp-bridge
    $ mkdir tcp-bridge/build && cd tcp-bridge/build
    $ cmake ..
    $ cmake --build . --target install
```

By default on a Linux system this will install into `/usr/local/bin`

