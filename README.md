# JPEG Decoder
Decodes JPEG images in baselnie mode and handles any errors in the image's code. Function `Decode` accepts the image's file path and returns an object of type `Image`, which can be converted to PNG format. Supports markers `SOI`, `SOF0`, `APPn`, `EOI`, `SOS`, `COM`, `DHT` and `DQT`.
